/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algorithm>
#include <list>
#include <stdexcept>

#include "backend.hpp"
#include "common/bit.hpp"

/**
 * TODO:
 * - reuse registers that expire in the same operation if possible
 * - think of a way to merge memory operands into X64 opcodes across IR opcodes.
 * - ...
 */

#define DESTRUCTURE_CONTEXT auto& [code, reg_alloc, state, location] = context;

namespace lunatic {
namespace backend {

using namespace Xbyak::util;

#ifdef _WIN64
  #define ABI_MSVC
#else
  #define ABI_SYSV
#endif

#ifdef ABI_MSVC
  static constexpr Xbyak::Reg64 kRegArg0 = rcx;
  static constexpr Xbyak::Reg64 kRegArg1 = rdx;
  static constexpr Xbyak::Reg64 kRegArg2 = r8;
  static constexpr Xbyak::Reg64 kRegArg3 = r9;
#else
  static constexpr Xbyak::Reg64 kRegArg0 = rdi;
  static constexpr Xbyak::Reg64 kRegArg1 = rsi;
  static constexpr Xbyak::Reg64 kRegArg2 = rdx;
  static constexpr Xbyak::Reg64 kRegArg3 = rcx;
#endif

static auto ReadByte(Memory& memory, u32 address, Memory::Bus bus) -> u8 {
  return memory.ReadByte(address, bus);
}

static auto ReadHalf(Memory& memory, u32 address, Memory::Bus bus) -> u16 {
  return memory.ReadHalf(address, bus);
}

static auto ReadWord(Memory& memory, u32 address, Memory::Bus bus) -> u32 {
  return memory.ReadWord(address, bus);
}

static void WriteByte(Memory& memory, u32 address, Memory::Bus bus, u8 value) {
  memory.WriteByte(address, value, bus);
}

static void WriteHalf(Memory& memory, u32 address, Memory::Bus bus, u16 value) {
  memory.WriteHalf(address, value, bus);
}

static void WriteWord(Memory& memory, u32 address, Memory::Bus bus, u32 value) {
  memory.WriteWord(address, value, bus);
}

X64Backend::X64Backend(
  Memory& memory,
  State& state,
  BasicBlockCache const& block_cache,
  bool const& irq_line
)   : memory(memory)
    , state(state)
    , block_cache(block_cache)
    , irq_line(irq_line) {
  BuildConditionTable();
  EmitCallBlock();
}

void X64Backend::BuildConditionTable() {
  for (int flags = 0; flags < 16; flags++) {
    bool n = flags & 8;
    bool z = flags & 4;
    bool c = flags & 2;
    bool v = flags & 1;

    condition_table[int(Condition::EQ)][flags] =  z;
    condition_table[int(Condition::NE)][flags] = !z;
    condition_table[int(Condition::CS)][flags] =  c;
    condition_table[int(Condition::CC)][flags] = !c;
    condition_table[int(Condition::MI)][flags] =  n;
    condition_table[int(Condition::PL)][flags] = !n;
    condition_table[int(Condition::VS)][flags] =  v;
    condition_table[int(Condition::VC)][flags] = !v;
    condition_table[int(Condition::HI)][flags] =  c && !z;
    condition_table[int(Condition::LS)][flags] = !c ||  z;
    condition_table[int(Condition::GE)][flags] = n == v;
    condition_table[int(Condition::LT)][flags] = n != v;
    condition_table[int(Condition::GT)][flags] = !(z || (n != v));
    condition_table[int(Condition::LE)][flags] =  (z || (n != v));
    condition_table[int(Condition::AL)][flags] = true;
    condition_table[int(Condition::NV)][flags] = false;
  }
}

void X64Backend::EmitCallBlock() {
  auto stack_displacement = sizeof(u64) + X64RegisterAllocator::kSpillAreaSize * sizeof(u32);

  Push(code, {rbx, rbp, r12, r13, r14, r15});
#ifdef ABI_MSVC
  Push(code, {rsi, rdi});
#endif
  code.sub(rsp, stack_displacement);
  code.mov(rbp, rsp);

  code.mov(r12, kRegArg0); // r12 = function pointer
  code.mov(rbx, kRegArg1); // rbx = cycle counter
    
  // Load carry flag into AH
  code.mov(rcx, uintptr(&state));
  code.mov(edx, dword[rcx + state.GetOffsetToCPSR()]);
  code.bt(edx, 29); // CF = value of bit 29
  code.lahf();
  
  code.call(r12);

  // Return remaining number of cycles
  code.mov(rax, rbx);

  code.add(rsp, stack_displacement);
#ifdef ABI_MSVC
  Pop(code, {rsi, rdi});
#endif
  Pop(code, {rbx, rbp, r12, r13, r14, r15});
  code.ret();

  CallBlock = code.getCode<int (*)(BasicBlock::CompiledFn, int)>();
}

void X64Backend::Compile(BasicBlock& basic_block) {
  auto code = new Xbyak::CodeGenerator{};
  auto label_return_to_dispatch = Xbyak::Label{};
  auto opcode_size = basic_block.key.Thumb() ? sizeof(u16) : sizeof(u32);

  for (auto const& micro_block : basic_block.micro_blocks) {
    auto& emitter  = micro_block.emitter;
    auto condition = micro_block.condition;
    auto reg_alloc = X64RegisterAllocator{emitter, *code};
    auto location = 0;
    auto context  = CompileContext{*code, reg_alloc, state, location};

    auto label_skip = Xbyak::Label{};
    auto label_done = Xbyak::Label{};

    // Skip micro block if ??ts condition is not met.
    if (condition != Condition::AL) {
      // TODO: can we perform a conditional jump based on eflags?
      code->mov(r8, u64(&condition_table[static_cast<int>(condition)]));
      code->mov(edx, dword[rcx + state.GetOffsetToCPSR()]);
      code->shr(edx, 28);
      code->cmp(byte[r8 + rdx], 0);
      code->je(label_skip, Xbyak::CodeGenerator::T_NEAR);
    }

    for (auto const& op : emitter.Code()) {
      reg_alloc.SetCurrentLocation(location++);
      CompileIROp(context, op);
    }

    if (condition != Condition::AL) {
      code->jmp(label_done);

      // If the micro block was skipped advance PC by the number of instructions in it. 
      code->L(label_skip);
      code->add(
        dword[rcx + state.GetOffsetToGPR(Mode::User, GPR::PC)],
        micro_block.length * opcode_size
      );

      code->L(label_done);
    }
  }

  // Return to the dispatcher if we ran out of cycles.
  code->sub(rbx, basic_block.length);
  code->jle(label_return_to_dispatch);

  // Return to the dispatcher if there is an IRQ to handle
  code->mov(rdx, uintptr(&irq_line));
  code->cmp(byte[rdx], 0);
  code->jnz(label_return_to_dispatch);

  // Build the block key from R15 and CPSR.
  // See frontend/basic_block.hpp
  code->mov(edx, dword[rcx + state.GetOffsetToGPR(Mode::User, GPR::PC)]);
  code->mov(esi, dword[rcx + state.GetOffsetToCPSR()]);
  code->shr(edx, 1);
  code->and_(esi, 0x3F);
  code->shl(rsi, 31);
  code->or_(rdx, rsi);

  // Split key into key0 and key1
  // TODO: can we exploit how the block lookup works?
  code->mov(rsi, rdx);
  code->shr(rsi, 19);
  code->and_(edx, 0x7FFFF);

  // Look block key up in the block cache.
  code->mov(rdi, uintptr(block_cache.data));
  code->mov(rdi, qword[rdi + rsi * sizeof(uintptr)]);
  code->cmp(rdi, 0);
  code->jz(label_return_to_dispatch); // fixme?
  code->mov(rdi, qword[rdi + rdx * sizeof(uintptr)]);
  code->cmp(rdi, 0);
  code->jz(label_return_to_dispatch); // fixme?
  code->mov(rdi, qword[rdi + offsetof(BasicBlock, function)]);
  code->jmp(rdi);

  code->L(label_return_to_dispatch);
  code->ret();

  basic_block.function = code->getCode<BasicBlock::CompiledFn>();
}

void X64Backend::CompileIROp(
  CompileContext const& context,
  std::unique_ptr<IROpcode> const& op
) {
  switch (op->GetClass()) {
    case IROpcodeClass::LoadGPR: CompileLoadGPR(context, lunatic_cast<IRLoadGPR>(op.get())); break;
    case IROpcodeClass::StoreGPR: CompileStoreGPR(context, lunatic_cast<IRStoreGPR>(op.get())); break;
    case IROpcodeClass::LoadSPSR: CompileLoadSPSR(context, lunatic_cast<IRLoadSPSR>(op.get())); break;
    case IROpcodeClass::StoreSPSR: CompileStoreSPSR(context, lunatic_cast<IRStoreSPSR>(op.get())); break;
    case IROpcodeClass::LoadCPSR: CompileLoadCPSR(context, lunatic_cast<IRLoadCPSR>(op.get())); break;
    case IROpcodeClass::StoreCPSR: CompileStoreCPSR(context, lunatic_cast<IRStoreCPSR>(op.get())); break;
    case IROpcodeClass::ClearCarry: CompileClearCarry(context, lunatic_cast<IRClearCarry>(op.get())); break;
    case IROpcodeClass::SetCarry:   CompileSetCarry(context, lunatic_cast<IRSetCarry>(op.get())); break;
    case IROpcodeClass::UpdateFlags: CompileUpdateFlags(context, lunatic_cast<IRUpdateFlags>(op.get())); break;
    case IROpcodeClass::UpdateSticky: CompileUpdateSticky(context, lunatic_cast<IRUpdateSticky>(op.get())); break;
    case IROpcodeClass::LSL: CompileLSL(context, lunatic_cast<IRLogicalShiftLeft>(op.get())); break;
    case IROpcodeClass::LSR: CompileLSR(context, lunatic_cast<IRLogicalShiftRight>(op.get())); break;
    case IROpcodeClass::ASR: CompileASR(context, lunatic_cast<IRArithmeticShiftRight>(op.get())); break;
    case IROpcodeClass::ROR: CompileROR(context, lunatic_cast<IRRotateRight>(op.get())); break;
    case IROpcodeClass::AND: CompileAND(context, lunatic_cast<IRBitwiseAND>(op.get())); break;
    case IROpcodeClass::BIC: CompileBIC(context, lunatic_cast<IRBitwiseBIC>(op.get())); break;
    case IROpcodeClass::EOR: CompileEOR(context, lunatic_cast<IRBitwiseEOR>(op.get())); break;
    case IROpcodeClass::SUB: CompileSUB(context, lunatic_cast<IRSub>(op.get())); break;
    case IROpcodeClass::RSB: CompileRSB(context, lunatic_cast<IRRsb>(op.get())); break;
    case IROpcodeClass::ADD: CompileADD(context, lunatic_cast<IRAdd>(op.get())); break;
    case IROpcodeClass::ADC: CompileADC(context, lunatic_cast<IRAdc>(op.get())); break;
    case IROpcodeClass::SBC: CompileSBC(context, lunatic_cast<IRSbc>(op.get())); break;
    case IROpcodeClass::RSC: CompileRSC(context, lunatic_cast<IRRsc>(op.get())); break;
    case IROpcodeClass::ORR: CompileORR(context, lunatic_cast<IRBitwiseORR>(op.get())); break;
    case IROpcodeClass::MOV: CompileMOV(context, lunatic_cast<IRMov>(op.get())); break;
    case IROpcodeClass::MVN: CompileMVN(context, lunatic_cast<IRMvn>(op.get())); break;
    case IROpcodeClass::MUL: CompileMUL(context, lunatic_cast<IRMultiply>(op.get())); break;
    case IROpcodeClass::ADD64: CompileADD64(context, lunatic_cast<IRAdd64>(op.get())); break;
    case IROpcodeClass::MemoryRead: CompileMemoryRead(context, lunatic_cast<IRMemoryRead>(op.get())); break;
    case IROpcodeClass::MemoryWrite: CompileMemoryWrite(context, lunatic_cast<IRMemoryWrite>(op.get())); break;
    case IROpcodeClass::Flush: CompileFlush(context, lunatic_cast<IRFlush>(op.get())); break;
    case IROpcodeClass::FlushExchange: CompileFlushExchange(context, lunatic_cast<IRFlushExchange>(op.get())); break;
    case IROpcodeClass::CLZ: CompileCLZ(context, lunatic_cast<IRCountLeadingZeros>(op.get())); break;
    case IROpcodeClass::QADD: CompileQADD(context, lunatic_cast<IRSaturatingAdd>(op.get())); break;
    case IROpcodeClass::QSUB: CompileQSUB(context, lunatic_cast<IRSaturatingSub>(op.get())); break;
    default: {
      throw std::runtime_error(
        fmt::format("X64Backend: unhandled IR opcode: {}", op->ToString())
      );
    }
  }
}

void X64Backend::Push(
  Xbyak::CodeGenerator& code,
  std::vector<Xbyak::Reg64> const& regs
) {
  for (auto reg : regs) {
    code.push(reg);
  }
}

void X64Backend::Pop(
  Xbyak::CodeGenerator& code,
  std::vector<Xbyak::Reg64> const& regs
) {
  auto end = regs.rend();

  for (auto it = regs.rbegin(); it != end; ++it) {
    code.pop(*it);
  }
}

void X64Backend::CompileLoadGPR(CompileContext const& context, IRLoadGPR* op) {
  DESTRUCTURE_CONTEXT;

  auto address  = rcx + state.GetOffsetToGPR(op->reg.mode, op->reg.reg);
  auto host_reg = reg_alloc.GetVariableHostReg(op->result);

  code.mov(host_reg, dword[address]);
}

void X64Backend::CompileStoreGPR(CompileContext const& context, IRStoreGPR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToGPR(op->reg.mode, op->reg.reg);

  if (op->value.IsConstant()) {
    code.mov(dword[address], op->value.GetConst().value);
  } else {
    auto host_reg = reg_alloc.GetVariableHostReg(op->value.GetVar());

    code.mov(dword[address], host_reg);
  }
}

void X64Backend::CompileLoadSPSR(CompileContext const& context, IRLoadSPSR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToSPSR(op->mode);
  auto host_reg = reg_alloc.GetVariableHostReg(op->result);

  code.mov(host_reg, dword[address]);
}

void X64Backend::CompileStoreSPSR(const CompileContext &context, IRStoreSPSR *op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToSPSR(op->mode);

  if (op->value.IsConstant()) {
    code.mov(dword[address], op->value.GetConst().value);
  } else {
    auto host_reg = reg_alloc.GetVariableHostReg(op->value.GetVar());

    code.mov(dword[address], host_reg);
  }
}

void X64Backend::CompileLoadCPSR(CompileContext const& context, IRLoadCPSR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToCPSR();
  auto host_reg = reg_alloc.GetVariableHostReg(op->result);

  code.mov(host_reg, dword[address]);
}

void X64Backend::CompileStoreCPSR(CompileContext const& context, IRStoreCPSR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToCPSR();

  if (op->value.IsConstant()) {
    code.mov(dword[address], op->value.GetConst().value);
  } else {
    auto host_reg = reg_alloc.GetVariableHostReg(op->value.GetVar());

    code.mov(dword[address], host_reg);
  }
}

void X64Backend::CompileClearCarry(CompileContext const& context, IRClearCarry* op) {
  context.code.and_(ah, ~1);
}

void X64Backend::CompileSetCarry(CompileContext const& context, IRSetCarry* op) {
  context.code.or_(ah, 1);
}

void X64Backend::CompileUpdateFlags(CompileContext const& context, IRUpdateFlags* op) {
  DESTRUCTURE_CONTEXT;

  u32 mask = 0;
  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto input_reg  = reg_alloc.GetVariableHostReg(op->input);

  if (op->flag_n) mask |= 0x80000000;
  if (op->flag_z) mask |= 0x40000000;
  if (op->flag_c) mask |= 0x20000000;
  if (op->flag_v) mask |= 0x10000000;

  auto pext_mask_reg = reg_alloc.GetTemporaryHostReg();
  auto flags_reg = reg_alloc.GetTemporaryHostReg();

  // Convert NZCV bits from AX register into the guest format.
  // Clear the bits which are not to be updated.
  code.mov(pext_mask_reg, 0xC101);
  code.pext(flags_reg, eax, pext_mask_reg);
  code.shl(flags_reg, 28);
  code.and_(flags_reg, mask);

  // Clear the bits to be updated, then OR the new values.
  code.mov(result_reg, input_reg);
  code.and_(result_reg, ~mask);
  code.or_(result_reg, flags_reg);
}

void X64Backend::CompileUpdateSticky(CompileContext const& context, IRUpdateSticky* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto input_reg  = reg_alloc.GetVariableHostReg(op->input);

  code.movzx(result_reg, al);
  code.shl(result_reg, 27);
  code.or_(result_reg, input_reg);
}

void X64Backend::CompileLSL(CompileContext const& context, IRLogicalShiftLeft* op) {
  DESTRUCTURE_CONTEXT;

  auto amount = op->amount;
  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto operand_reg = reg_alloc.GetVariableHostReg(op->operand);

  code.mov(result_reg, operand_reg);
  code.shl(result_reg.cvt64(), 32);

  if (amount.IsConstant()) {
    if (op->update_host_flags) {
      code.sahf();
    }
    code.shl(result_reg.cvt64(), u8(std::min(amount.GetConst().value, 33U)));
  } else {
    auto amount_reg = reg_alloc.GetVariableHostReg(op->amount.GetVar());

    code.push(rcx);
    code.mov(cl, 33);
    code.cmp(amount_reg.cvt8(), u8(33));
    code.cmovl(ecx, amount_reg);

    if (op->update_host_flags) {
      code.sahf();
    }
    code.shl(result_reg.cvt64(), cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }

  code.shr(result_reg.cvt64(), 32);
}

void X64Backend::CompileLSR(CompileContext const& context, IRLogicalShiftRight* op) {
  DESTRUCTURE_CONTEXT;

  auto amount = op->amount;
  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto operand_reg = reg_alloc.GetVariableHostReg(op->operand);

  code.mov(result_reg, operand_reg);

  if (amount.IsConstant()) {
    auto amount_value = amount.GetConst().value;

    // LSR #0 equals to LSR #32
    if (amount_value == 0) {
      amount_value = 32;
    }

    if (op->update_host_flags) {
      code.sahf();
    }

    code.shr(result_reg.cvt64(), u8(std::min(amount_value, 33U)));
  } else {
    auto amount_reg = reg_alloc.GetVariableHostReg(op->amount.GetVar());
    code.push(rcx);
    code.mov(cl, 33);
    code.cmp(amount_reg.cvt8(), u8(33));
    code.cmovl(ecx, amount_reg);
    if (op->update_host_flags) {
      code.sahf();
    }
    code.shr(result_reg.cvt64(), cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }
}

void X64Backend::CompileASR(CompileContext const& context, IRArithmeticShiftRight* op) {
  DESTRUCTURE_CONTEXT;

  auto amount = op->amount;
  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto operand_reg = reg_alloc.GetVariableHostReg(op->operand);

  // Mirror sign-bit in the upper 32-bit of the full 64-bit register.
  code.movsxd(result_reg.cvt64(), operand_reg);

  // TODO: change shift amount saturation from 33 to 32 for ASR? 32 would also work I guess?

  if (amount.IsConstant()) {
    auto amount_value = amount.GetConst().value;

    // ASR #0 equals to ASR #32
    if (amount_value == 0) {
      amount_value = 32;
    }

    if (op->update_host_flags) {
      code.sahf();
    }

    code.sar(result_reg.cvt64(), u8(std::min(amount_value, 33U)));
  } else {
    auto amount_reg = reg_alloc.GetVariableHostReg(op->amount.GetVar());
    code.push(rcx);
    code.mov(cl, 33);
    code.cmp(amount_reg.cvt8(), u8(33));
    code.cmovl(ecx, amount_reg);
    if (op->update_host_flags) {
      code.sahf();
    }
    code.sar(result_reg.cvt64(), cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }

  // Clear upper 32-bit of the result
  code.mov(result_reg, result_reg);
}

void X64Backend::CompileROR(CompileContext const& context, IRRotateRight* op) {
  DESTRUCTURE_CONTEXT;

  auto amount = op->amount;
  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto operand_reg = reg_alloc.GetVariableHostReg(op->operand);
  auto label_done = Xbyak::Label{};

  code.mov(result_reg, operand_reg);

  if (amount.IsConstant()) {
    auto amount_value = amount.GetConst().value;

    // ROR #0 equals to RRX #1
    if (amount_value == 0) {
      code.sahf();
      code.rcr(result_reg, 1);
    } else {
      if (op->update_host_flags) {
        code.sahf();
      }
      code.ror(result_reg, u8(amount_value));
    }
  } else {
    auto amount_reg = reg_alloc.GetVariableHostReg(op->amount.GetVar());
    auto label_ok = Xbyak::Label{};

    // Handle (amount % 32) == 0 and amount == 0 cases.
    if (op->update_host_flags) {
      code.test(amount_reg.cvt8(), 31);
      code.jnz(label_ok);

      code.cmp(amount_reg.cvt8(), 0);
      code.jz(label_done);

      code.bt(result_reg, 31);
      code.lahf();
      code.jmp(label_done);
    }

    code.L(label_ok);
    code.push(rcx);
    code.mov(cl, amount_reg.cvt8());
    code.ror(result_reg, cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }
  
  code.L(label_done);
}

void X64Backend::CompileAND(CompileContext const& context, IRBitwiseAND* op) {
  DESTRUCTURE_CONTEXT;

  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      code.test(lhs_reg, imm);
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.and_(result_reg, imm);
    }
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    if (op->result.IsNull()) {
      code.test(lhs_reg, rhs_reg);
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.and_(result_reg, rhs_reg);
    }
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileBIC(CompileContext const& context, IRBitwiseBIC* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    code.mov(result_reg, lhs_reg);
    code.and_(result_reg, ~imm);
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    code.mov(result_reg, rhs_reg);
    code.not_(result_reg);
    code.and_(result_reg, lhs_reg);
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileEOR(CompileContext const& context, IRBitwiseEOR* op) {
  DESTRUCTURE_CONTEXT;

  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      /* TODO: possible optimization when lhs_reg != rhs_reg
       *   xor lhs_reg, rhs_reg
       *   bt ax, 8
       *   lahf
       *   xor lhs_reg, rhs_reg
       */
      code.push(lhs_reg.cvt64());
      code.xor_(lhs_reg, imm);
      code.pop(lhs_reg.cvt64());
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.xor_(result_reg, imm);
    }
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    if (op->result.IsNull()) {
      /* TODO: possible optimization when lhs_reg != rhs_reg
       *   xor lhs_reg, rhs_reg
       *   bt ax, 8
       *   lahf
       *   xor lhs_reg, rhs_reg
       */
      code.push(lhs_reg.cvt64());
      code.xor_(lhs_reg, rhs_reg);
      code.pop(lhs_reg.cvt64());
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.xor_(result_reg, rhs_reg);
    }
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileSUB(CompileContext const& context, IRSub* op) {
  DESTRUCTURE_CONTEXT;

  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      code.cmp(lhs_reg, imm);
      code.cmc();
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.sub(result_reg, imm);
      code.cmc();
    }
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    if (op->result.IsNull()) {
      code.cmp(lhs_reg, rhs_reg);
      code.cmc();
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.sub(result_reg, rhs_reg);
      code.cmc();
    }
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileRSB(CompileContext const& context, IRRsb* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    code.mov(result_reg, imm);
    code.sub(result_reg, lhs_reg);
    code.cmc();
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    code.mov(result_reg, rhs_reg);
    code.sub(result_reg, lhs_reg);
    code.cmc();
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileADD(CompileContext const& context, IRAdd* op) {
  DESTRUCTURE_CONTEXT;

  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->result.IsNull() && !op->update_host_flags) {
    return;
  }

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      // eax will be trashed by lahf anyways
      code.mov(eax, lhs_reg);
      code.add(eax, imm);
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.add(result_reg, imm);
    }
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    if (op->result.IsNull()) {
      // eax will be trashed by lahf anyways
      code.mov(eax, lhs_reg);
      code.add(eax, rhs_reg);
    } else {
      auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());

      code.mov(result_reg, lhs_reg);
      code.add(result_reg, rhs_reg);
    }
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileADC(CompileContext const& context, IRAdc* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  code.sahf();

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    code.mov(result_reg, lhs_reg);
    code.adc(result_reg, imm);
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    code.mov(result_reg, lhs_reg);
    code.adc(result_reg, rhs_reg);
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileSBC(CompileContext const& context, IRSbc* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  code.sahf();
  code.cmc();

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    code.mov(result_reg, lhs_reg);
    code.sbb(result_reg, imm);
    code.cmc();
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    code.mov(result_reg, lhs_reg);
    code.sbb(result_reg, rhs_reg);
    code.cmc();
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileRSC(CompileContext const& context, IRRsc* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  code.sahf();
  code.cmc();

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    code.mov(result_reg, imm);
    code.sbb(result_reg, lhs_reg);
    code.cmc();
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    code.mov(result_reg, rhs_reg);
    code.sbb(result_reg, lhs_reg);
    code.cmc();
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileORR(CompileContext const& context, IRBitwiseORR* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Unwrap());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    code.mov(result_reg, lhs_reg);
    code.or_(result_reg, imm);
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    code.mov(result_reg, lhs_reg);
    code.or_(result_reg, rhs_reg);
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileMOV(CompileContext const& context, IRMov* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result);

  if (op->source.IsConstant()) {
    code.mov(result_reg, op->source.GetConst().value);
  } else {
    code.mov(result_reg, reg_alloc.GetVariableHostReg(op->source.GetVar()));
  }

  if (op->update_host_flags) {
    code.test(result_reg, result_reg);
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileMVN(CompileContext const& context, IRMvn* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result);

  if (op->source.IsConstant()) {
    code.mov(result_reg, op->source.GetConst().value);
  } else {
    code.mov(result_reg, reg_alloc.GetVariableHostReg(op->source.GetVar()));
  }

  code.not_(result_reg);

  if (op->update_host_flags) {
    code.test(result_reg, result_reg);
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileMUL(CompileContext const& context, IRMultiply* op) {
  DESTRUCTURE_CONTEXT;

  auto result_lo_reg = reg_alloc.GetVariableHostReg(op->result_lo);
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);
  auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs);

  if (op->result_hi.HasValue()) {
    auto result_hi_reg = reg_alloc.GetVariableHostReg(op->result_hi.Unwrap());
    auto rhs_ext_reg = reg_alloc.GetTemporaryHostReg().cvt64();

    if (op->lhs.data_type == IRDataType::SInt32) {
      code.movsxd(result_hi_reg.cvt64(), lhs_reg);
      code.movsxd(rhs_ext_reg, rhs_reg);
    } else {
      code.mov(result_hi_reg, lhs_reg);
      code.mov(rhs_ext_reg.cvt32(), rhs_reg);
    }

    code.imul(result_hi_reg.cvt64(), rhs_ext_reg);

    if (op->update_host_flags) {
      code.test(result_hi_reg.cvt64(), result_hi_reg.cvt64());
      code.lahf();
    }

    code.mov(result_lo_reg, result_hi_reg);
    code.shr(result_hi_reg.cvt64(), 32);
  } else {
    code.mov(result_lo_reg, lhs_reg);
    code.imul(result_lo_reg, rhs_reg);

    if (op->update_host_flags) {
      code.test(result_lo_reg, result_lo_reg);
      code.lahf();
    }
  }
}

void X64Backend::CompileADD64(CompileContext const& context, IRAdd64* op) {
  DESTRUCTURE_CONTEXT;

  auto result_hi_reg = reg_alloc.GetVariableHostReg(op->result_hi);
  auto result_lo_reg = reg_alloc.GetVariableHostReg(op->result_lo);
  auto lhs_hi_reg = reg_alloc.GetVariableHostReg(op->lhs_hi);
  auto lhs_lo_reg = reg_alloc.GetVariableHostReg(op->lhs_lo);
  auto rhs_hi_reg = reg_alloc.GetVariableHostReg(op->rhs_hi);
  auto rhs_lo_reg = reg_alloc.GetVariableHostReg(op->rhs_lo);

  if (op->update_host_flags) {
    // Pack (lhs_hi, lhs_lo) into result_hi
    code.mov(result_hi_reg, lhs_hi_reg);
    code.shl(result_hi_reg.cvt64(), 32);
    code.or_(result_hi_reg.cvt64(), lhs_lo_reg);

    // Pack (rhs_hi, rhs_lo) into result_lo
    code.mov(result_lo_reg, rhs_hi_reg);
    code.shl(result_lo_reg.cvt64(), 32);
    code.or_(result_lo_reg.cvt64(), rhs_lo_reg);

    code.add(result_hi_reg.cvt64(), result_lo_reg.cvt64());
    code.lahf();
  
    code.mov(result_lo_reg, result_hi_reg);
    code.shr(result_hi_reg.cvt64(), 32);
  } else {
    code.mov(result_lo_reg, lhs_lo_reg);
    code.mov(result_hi_reg, lhs_hi_reg);

    code.add(result_lo_reg, rhs_lo_reg);
    code.adc(result_hi_reg, rhs_hi_reg);
  }
}

void X64Backend::CompileMemoryRead(CompileContext const& context, IRMemoryRead* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto address_reg = reg_alloc.GetVariableHostReg(op->address);
  auto flags = op->flags;

  auto label_slowmem = Xbyak::Label{};
  auto label_final = Xbyak::Label{};
  auto pagetable = memory.pagetable.get();

  // TODO: properly allocate a free register.
  // Or statically allocate a register for the page table pointer?
  code.push(rcx);

  if (pagetable != nullptr) {
    code.mov(rcx, u64(pagetable));

    // TODO: check for DTCM!!!

    // Get the page table entry
    code.mov(result_reg, address_reg);
    code.shr(result_reg, Memory::kPageShift);
    code.mov(rcx, qword[rcx + result_reg.cvt64() * sizeof(uintptr)]);

    // Check if the entry is a null pointer.
    code.test(rcx, rcx);
    code.jz(label_slowmem);

    code.mov(result_reg, address_reg);

    if (flags & Word) {
      code.and_(result_reg, Memory::kPageMask & ~3);
      code.mov(result_reg, dword[rcx + result_reg.cvt64()]);
    }

    if (flags & Half) {
      code.and_(result_reg, Memory::kPageMask & ~1);
      if (flags & Signed) {
        code.movsx(result_reg, word[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, word[rcx + result_reg.cvt64()]);
      }
    }

    if (flags & Byte) {
      code.and_(result_reg, Memory::kPageMask);
      if (flags & Signed) {
        code.movsx(result_reg, byte[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, byte[rcx + result_reg.cvt64()]);
      }
    }

    code.jmp(label_final);
  }

  code.L(label_slowmem);

  // TODO: determine which registers need to be saved.
  Push(code, {rax, rdx, r8, r9, r10, r11});
#ifdef ABI_SYSV
  Push(code, {rsi, rdi});
#endif

  code.mov(kRegArg1.cvt32(), address_reg);

  if (flags & Word) {
    code.and_(kRegArg1.cvt32(), ~3);
    code.mov(rax, uintptr(&ReadWord));
  }

  if (flags & Half) {
    code.and_(kRegArg1.cvt32(), ~1);
    code.mov(rax, uintptr(&ReadHalf));
  }

  if (flags & Byte) {
    code.mov(rax, uintptr(&ReadByte));
  }

  code.mov(kRegArg0, uintptr(&memory));
  code.mov(kRegArg2.cvt32(), u32(Memory::Bus::Data));
  code.sub(rsp, 0x20);
  code.call(rax);
  code.add(rsp, 0x20);

#ifdef ABI_SYSV
  Pop(code, {rsi, rdi});
#endif
  Pop(code, {rdx, r8, r9, r10, r11});

  if (flags & Word) {
    code.mov(result_reg, eax);
  }

  if (flags & Half) {
    if (flags & Signed) {
      code.movsx(result_reg, ax);
    } else {
      code.movzx(result_reg, ax);
    }
  }

  if (flags & Byte) {
    if (flags & Signed) {
      code.movsx(result_reg, al);
    } else {
      code.movzx(result_reg, al);
    }
  }

  code.pop(rax);

  code.L(label_final);

  if (flags & Rotate) {
    if (flags & Word) {
      code.mov(ecx, address_reg);
      code.and_(cl, 3);
      code.shl(cl, 3);
      code.ror(result_reg, cl);
    }

    if (flags & Half) {
      code.mov(ecx, address_reg);
      code.and_(cl, 1);
      code.shl(cl, 3);
      code.ror(result_reg, cl);
    }
  }

  static constexpr auto kHalfSignedARMv4T = Half | Signed | ARMv4T;

  // ARM7TDMI/ARMv4T special case: unaligned LDRSH is effectively LDRSB.
  // TODO: this can probably be optimized by checking for misalignment early.
  if ((flags & kHalfSignedARMv4T) == kHalfSignedARMv4T) {
    auto label_aligned = Xbyak::Label{};

    code.bt(address_reg, 0);
    code.jnc(label_aligned);
    code.shr(result_reg, 8);
    code.movsx(result_reg, result_reg.cvt8());
    code.L(label_aligned);
  }

  code.pop(rcx);
}

void X64Backend::CompileMemoryWrite(CompileContext const& context, IRMemoryWrite* op) {
  DESTRUCTURE_CONTEXT;

  auto source_reg  = reg_alloc.GetVariableHostReg(op->source);
  auto address_reg = reg_alloc.GetVariableHostReg(op->address);
  auto scratch_reg = reg_alloc.GetTemporaryHostReg();
  auto flags = op->flags;

  auto label_slowmem = Xbyak::Label{};
  auto label_final = Xbyak::Label{};
  auto pagetable = memory.pagetable.get();

  code.push(rcx);

  if (pagetable != nullptr) {
    code.mov(rcx, u64(pagetable));

    // TODO: check for ITCM and DTCM!!!

    // Get the page table entry
    code.mov(scratch_reg, address_reg);
    code.shr(scratch_reg, Memory::kPageShift);
    code.mov(rcx, qword[rcx + scratch_reg.cvt64() * sizeof(uintptr)]);

    // Check if the entry is a null pointer.
    code.test(rcx, rcx);
    code.jz(label_slowmem);

    code.mov(scratch_reg, address_reg);

    if (flags & Word) {
      code.and_(scratch_reg, Memory::kPageMask & ~3);
      code.mov(dword[rcx + scratch_reg.cvt64()], source_reg);
    }

    if (flags & Half) {
      code.and_(scratch_reg, Memory::kPageMask & ~1);
      code.mov(word[rcx + scratch_reg.cvt64()], source_reg.cvt16());
    }

    if (flags & Byte) {
      code.and_(scratch_reg, Memory::kPageMask);
      code.mov(byte[rcx + scratch_reg.cvt64()], source_reg.cvt8());
    }

    code.jmp(label_final);
  }

  code.L(label_slowmem);

  // TODO: determine which registers need to be saved.
  Push(code, {rax, rdx, r8, r9, r10, r11});
#ifdef ABI_SYSV
  Push(code, {rsi, rdi});
#endif

  if (kRegArg1.cvt32() == source_reg) {
    code.mov(kRegArg3.cvt32(), address_reg);
    code.xchg(kRegArg1.cvt32(), kRegArg3.cvt32());

    if (flags & Half) {
      code.movzx(kRegArg3.cvt32(), kRegArg3.cvt16());
    }

    if (flags & Byte) {
      code.movzx(kRegArg3.cvt32(), kRegArg3.cvt8());
    }
  } else {
    code.mov(kRegArg1.cvt32(), address_reg);

    if (flags & Word) {
      code.mov(kRegArg3.cvt32(), source_reg);
    }

    if (flags & Half) {
      code.movzx(kRegArg3.cvt32(), source_reg.cvt16());
    }

    if (flags & Byte) {
      code.movzx(kRegArg3.cvt32(), source_reg.cvt8());
    }
  }

  if (flags & Word) {
    code.and_(kRegArg1.cvt32(), ~3);
    code.mov(rax, uintptr(&WriteWord));
  }

  if (flags & Half) {
    code.and_(kRegArg1.cvt32(), ~1);
    code.mov(rax, uintptr(&WriteHalf));
  }

  if (flags & Byte) {
    code.mov(rax, uintptr(&WriteByte));
  }

  code.mov(kRegArg0, uintptr(&memory));
  code.mov(kRegArg2.cvt32(), u32(Memory::Bus::Data));
  code.sub(rsp, 0x20);
  code.call(rax);
  code.add(rsp, 0x20);

#ifdef ABI_SYSV
  Pop(code, {rsi, rdi});
#endif
  Pop(code, {rax, rdx, r8, r9, r10, r11});

  code.L(label_final);
  code.pop(rcx);
}

void X64Backend::CompileFlush(CompileContext const& context, IRFlush* op) {
  DESTRUCTURE_CONTEXT;

  auto cpsr_reg = reg_alloc.GetVariableHostReg(op->cpsr_in);
  auto r15_in_reg = reg_alloc.GetVariableHostReg(op->address_in);
  auto r15_out_reg = reg_alloc.GetVariableHostReg(op->address_out);

  // Thanks to @wheremyfoodat (github.com/wheremyfoodat) for coming up with this.
  code.test(cpsr_reg, 1 << 5);
  code.sete(r15_out_reg.cvt8());
  code.movzx(r15_out_reg, r15_out_reg.cvt8());
  code.lea(r15_out_reg, dword[r15_in_reg + r15_out_reg * 4 + 4]);
}

void X64Backend::CompileFlushExchange(const CompileContext &context, IRFlushExchange *op) {
  DESTRUCTURE_CONTEXT;

  auto address_out_reg = reg_alloc.GetVariableHostReg(op->address_out);
  auto address_in_reg = reg_alloc.GetVariableHostReg(op->address_in);
  auto cpsr_out_reg = reg_alloc.GetVariableHostReg(op->cpsr_out);
  auto cpsr_in_reg = reg_alloc.GetVariableHostReg(op->cpsr_in);

  auto label_arm = Xbyak::Label{};
  auto label_done = Xbyak::Label{};

  // TODO: fix this horrible codegen.
  // a) try to make this branch-less
  // b) do we always need to mask with ~1 / ~3?

  code.mov(address_out_reg, address_in_reg);
  code.mov(cpsr_out_reg, cpsr_in_reg);

  code.test(address_in_reg, 1);
  code.je(label_arm);

  // Thumb
  code.or_(cpsr_out_reg, 1 << 5);
  code.and_(address_out_reg, ~1);
  code.add(address_out_reg, sizeof(u16) * 2);
  code.jmp(label_done);

  // ARM
  code.L(label_arm);
  code.and_(cpsr_out_reg, ~(1 << 5));
  code.and_(address_out_reg, ~3);
  code.add(address_out_reg, sizeof(u32) * 2);

  code.L(label_done);
}

void X64Backend::CompileCLZ(CompileContext const& context, IRCountLeadingZeros* op) {
  DESTRUCTURE_CONTEXT;
  code.lzcnt(
    reg_alloc.GetVariableHostReg(op->result),
    reg_alloc.GetVariableHostReg(op->operand)
  );
}

void X64Backend::CompileQADD(CompileContext const& context, IRSaturatingAdd* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);
  auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs);
  auto temp_reg = reg_alloc.GetTemporaryHostReg();
  auto label_skip_saturate = Xbyak::Label{};

  code.mov(result_reg, lhs_reg);
  code.add(result_reg, rhs_reg);
  code.jno(label_skip_saturate);

  code.mov(temp_reg, 0x7FFF'FFFF);
  code.mov(result_reg, 0x8000'0000);
  code.cmovs(result_reg, temp_reg);

  code.L(label_skip_saturate);
  code.seto(al);
}

void X64Backend::CompileQSUB(CompileContext const& context, IRSaturatingSub* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result);
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs);
  auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs);
  auto temp_reg = reg_alloc.GetTemporaryHostReg();
  auto label_skip_saturate = Xbyak::Label{};

  code.mov(result_reg, lhs_reg);
  code.sub(result_reg, rhs_reg);
  code.jno(label_skip_saturate);

  code.mov(temp_reg, 0x7FFF'FFFF);
  code.mov(result_reg, 0x8000'0000);
  code.cmovs(result_reg, temp_reg);

  code.L(label_skip_saturate);
  code.seto(al);
}

} // namespace lunatic::backend
} // namespace lunatic
