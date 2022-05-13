/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algorithm>
#include <cstdlib>
#include <list>
#include <stdexcept>

#include "backend.hpp"
#include "common.hpp"
#include "common/aligned_memory.hpp"
#include "common/bit.hpp"
#include "vtune.hpp"

/**
 * TODO:
 * - think of a way to merge memory operands into X64 opcodes across IR opcodes.
 * - ...
 */

namespace lunatic {
namespace backend {

X64Backend::X64Backend(
  CPU::Descriptor const& descriptor,
  State& state,
  BasicBlockCache& block_cache,
  bool const& irq_line
)   : memory(descriptor.memory)
    , state(state)
    , coprocessors(descriptor.coprocessors)
    , block_cache(block_cache)
    , irq_line(irq_line) {
  CreateCodeGenerator();
  EmitCallBlock();
}

X64Backend::~X64Backend() {
  delete code;
  memory::free(buffer);
}

void X64Backend::CreateCodeGenerator() {
  buffer = reinterpret_cast<u8*>(memory::aligned_alloc(4096, kCodeBufferSize));

  if (buffer == nullptr) {
    throw std::runtime_error(
      fmt::format("lunatic: failed to allocate memory for JIT compilation")
    );
  }

  Xbyak::CodeArray::protect(
    buffer,
    kCodeBufferSize,
    Xbyak::CodeArray::PROTECT_RWE
  );

  code = new Xbyak::CodeGenerator{kCodeBufferSize, buffer};
}

void X64Backend::EmitCallBlock() {
  auto stack_displacement = sizeof(u64) + X64RegisterAllocator::kSpillAreaSize * sizeof(u32);

  CallBlock = (int (*)(BasicBlock::CompiledFn, int))code->getCurr();

  Push(*code, {rbx, rbp, r12, r13, r14, r15});
#ifdef ABI_MSVC
  Push(*code, {rsi, rdi});
#endif
  code->sub(rsp, stack_displacement);
  code->mov(rbp, rsp);

  code->mov(r12, kRegArg0); // r12 = function pointer
  code->mov(rbx, kRegArg1); // rbx = cycle counter

  // Load carry flag into AH
  code->mov(rcx, uintptr(&state));
  code->mov(edx, dword[rcx + state.GetOffsetToCPSR()]);
  code->bt(edx, 29); // CF = value of bit 29
  code->lahf();
  
  code->call(r12);

  // Return remaining number of cycles
  code->mov(rax, rbx);

  code->add(rsp, stack_displacement);
#ifdef ABI_MSVC
  Pop(*code, {rsi, rdi});
#endif
  Pop(*code, {rbx, rbp, r12, r13, r14, r15});
  code->ret();

#if LUNATIC_USE_VTUNE
  vtune::ReportCallBlock(reinterpret_cast<u8*>(CallBlock), code->getCurr());
#endif
}

void X64Backend::Compile(BasicBlock& basic_block) {
  try {
    auto label_return_to_dispatch = Xbyak::Label{};
    auto opcode_size = basic_block.key.Thumb() ? sizeof(u16) : sizeof(u32);
    auto number_of_micro_blocks = basic_block.micro_blocks.size();

    basic_block.function = (BasicBlock::CompiledFn)code->getCurr();

    for (size_t i = 0; i < number_of_micro_blocks; i++) {
      auto const& micro_block = basic_block.micro_blocks[i];
      auto& emitter  = micro_block.emitter;
      auto condition = micro_block.condition;
      auto reg_alloc = X64RegisterAllocator{emitter, *code};
      auto context   = CompileContext{*code, reg_alloc, state};

      auto label_skip = Xbyak::Label{};
      auto label_done = Xbyak::Label{};

      // Skip past the micro block if its condition is not met
      EmitConditionalBranch(condition, label_skip);

      // Compile each IR opcode inside the micro block
      for (auto const& op : emitter.Code()) {
        CompileIROp(context, op);
        reg_alloc.AdvanceLocation();
      }

      /* Once we reached the end of the basic block,
       * check if we can emit a jump to an already compiled basic block.
       * Also update the cycle counter in that case and return to the dispatcher
       * in the case that we ran out of cycles.
       */
      if (basic_block.enable_fast_dispatch && i == number_of_micro_blocks - 1) {
        auto& branch_target = basic_block.branch_target;

        if (branch_target.key.value != 0) {
          auto target_block = block_cache.Get(branch_target.key);

          if (target_block != nullptr) {
            // Return to the dispatcher if we ran out of cycles.
            code->sub(rbx, basic_block.length);
            code->jle(label_return_to_dispatch, Xbyak::CodeGenerator::T_NEAR);

            // Return to the dispatcher if there is an IRQ to handle
            code->mov(rdx, uintptr(&irq_line));
            code->cmp(byte[rdx], 0);
            code->jnz(label_return_to_dispatch);

            code->mov(rsi, u64(target_block->function));
            code->jmp(rsi);
          }
        }
      }

      /* The program counter is normally updated via IR opcodes.
       * But if we skipped past the code which'd do that, we need to manually
       * update the program counter.
       */
      if (condition != Condition::AL) {
        code->jmp(label_done);

        code->L(label_skip);
        code->add(
          dword[rcx + state.GetOffsetToGPR(Mode::User, GPR::PC)],
          micro_block.length * opcode_size
        );

        code->L(label_done);
      }
    }

    if (basic_block.enable_fast_dispatch) {
      // Return to the dispatcher if we ran out of cycles.
      code->sub(rbx, basic_block.length);
      code->jle(label_return_to_dispatch);

      // Return to the dispatcher if there is an IRQ to handle
      code->mov(rdx, uintptr(&irq_line));
      code->cmp(byte[rdx], 0);
      code->jnz(label_return_to_dispatch);

      // If the next basic block already is compiled then jump to it.
      EmitBasicBlockDispatch(label_return_to_dispatch);

      code->L(label_return_to_dispatch);
      code->ret();
    } else {
      code->sub(rbx, basic_block.length);
      code->ret();
    }

#if LUNATIC_USE_VTUNE
    vtune::ReportBasicBlock(basic_block, code->getCurr());
#endif
  } catch (Xbyak::Error error) {
    if (int(error) == Xbyak::ERR_CODE_IS_TOO_BIG) {
      fmt::print("FLUSH\n");
      block_cache.Flush();
      code->resetSize();
      EmitCallBlock();
      Compile(basic_block);
    } else {
      throw;
    }
  }
}

void X64Backend::EmitConditionalBranch(Condition condition, Xbyak::Label& label_skip) {
  if (condition == Condition::AL) {
    return;
  }

  // TODO: Keep decompressed flags in eax?
  code->mov(eax, dword[rcx + state.GetOffsetToCPSR()]);
  code->shr(eax, 28);
  code->mov(edx, 0xC101);
  code->pdep(eax, eax, edx);

  switch (condition) {
    case Condition::EQ:
      code->sahf();
      code->jnz(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::NE:
      code->sahf();
      code->jz(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::CS:
      code->sahf();
      code->jnc(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::CC:
      code->sahf();
      code->jc(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::MI:
      code->sahf();
      code->jns(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::PL:
      code->sahf();
      code->js(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::VS:
      code->cmp(al, 0x81);
      code->jno(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::VC:
      code->cmp(al, 0x81);
      code->jo(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::HI:
      code->sahf();
      code->cmc();
      code->jna(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::LS:
      code->sahf();
      code->cmc();
      code->ja(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::GE:
      code->cmp(al, 0x81);
      code->sahf();
      code->jnge(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::LT:
      code->cmp(al, 0x81);
      code->sahf();
      code->jnl(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::GT:
      code->cmp(al, 0x81);
      code->sahf();
      code->jng(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::LE:
      code->cmp(al, 0x81);
      code->sahf();
      code->jnle(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::NV:
      code->jmp(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
  }
}

void X64Backend::EmitBasicBlockDispatch(Xbyak::Label& label_cache_miss) {
  // Build the block key from R15 and CPSR.
  // See frontend/basic_block.hpp
  code->mov(edx, dword[rcx + state.GetOffsetToGPR(Mode::User, GPR::PC)]);
  code->mov(esi, dword[rcx + state.GetOffsetToCPSR()]);
  code->shr(edx, 1);
  code->and_(esi, 0x3F);
  code->shl(rsi, 31);
  code->or_(rdx, rsi);

  // Hash0 lookup (first level)
  code->mov(rsi, rdx);
  code->shr(rsi, 19);
  code->mov(rdi, uintptr(block_cache.data));
  code->mov(rdi, qword[rdi + rsi * sizeof(uintptr)]);
  code->test(rdi, rdi);
  code->jz(label_cache_miss);

  // Hash1 lookup (second level)
  code->and_(edx, 0x7FFFF);
  code->mov(rdi, qword[rdi + rdx * sizeof(uintptr)]);
  code->test(rdi, rdi);
  code->jz(label_cache_miss);
  code->mov(rdi, qword[rdi + offsetof(BasicBlock, function)]);

  // Load carry flag into AH
  code->mov(edx, dword[rcx + state.GetOffsetToCPSR()]);
  code->bt(edx, 29); // CF = value of bit 29
  code->lahf();

  code->jmp(rdi);
}

void X64Backend::CompileIROp(
  CompileContext const& context,
  std::unique_ptr<IROpcode> const& op
) {
  switch (op->GetClass()) {
    // Context access (compile_context.cpp)
    case IROpcodeClass::LoadGPR: CompileLoadGPR(context, lunatic_cast<IRLoadGPR>(op.get())); break;
    case IROpcodeClass::StoreGPR: CompileStoreGPR(context, lunatic_cast<IRStoreGPR>(op.get())); break;
    case IROpcodeClass::LoadSPSR: CompileLoadSPSR(context, lunatic_cast<IRLoadSPSR>(op.get())); break;
    case IROpcodeClass::StoreSPSR: CompileStoreSPSR(context, lunatic_cast<IRStoreSPSR>(op.get())); break;
    case IROpcodeClass::LoadCPSR: CompileLoadCPSR(context, lunatic_cast<IRLoadCPSR>(op.get())); break;
    case IROpcodeClass::StoreCPSR: CompileStoreCPSR(context, lunatic_cast<IRStoreCPSR>(op.get())); break;

    // Processor flags (compile_flags.cpp)
    case IROpcodeClass::ClearCarry: CompileClearCarry(context, lunatic_cast<IRClearCarry>(op.get())); break;
    case IROpcodeClass::SetCarry:   CompileSetCarry(context, lunatic_cast<IRSetCarry>(op.get())); break;
    case IROpcodeClass::UpdateFlags: CompileUpdateFlags(context, lunatic_cast<IRUpdateFlags>(op.get())); break;
    case IROpcodeClass::UpdateSticky: CompileUpdateSticky(context, lunatic_cast<IRUpdateSticky>(op.get())); break;
    case IROpcodeClass::UpdateGE: CompileUpdateGE(context, lunatic_cast<IRUpdateGE>(op.get())); break;
    
    // Barrel shifter (compile_shift.cpp)
    case IROpcodeClass::LSL: CompileLSL(context, lunatic_cast<IRLogicalShiftLeft>(op.get())); break;
    case IROpcodeClass::LSR: CompileLSR(context, lunatic_cast<IRLogicalShiftRight>(op.get())); break;
    case IROpcodeClass::ASR: CompileASR(context, lunatic_cast<IRArithmeticShiftRight>(op.get())); break;
    case IROpcodeClass::ROR: CompileROR(context, lunatic_cast<IRRotateRight>(op.get())); break;
    
    // ALU (compile_alu.cpp)
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
    case IROpcodeClass::CLZ: CompileCLZ(context, lunatic_cast<IRCountLeadingZeros>(op.get())); break;
    case IROpcodeClass::QADD: CompileQADD(context, lunatic_cast<IRSaturatingAdd>(op.get())); break;
    case IROpcodeClass::QSUB: CompileQSUB(context, lunatic_cast<IRSaturatingSub>(op.get())); break;

    // Multiply (and accumulate) (compile_multiply.cpp)
    case IROpcodeClass::MUL: CompileMUL(context, lunatic_cast<IRMultiply>(op.get())); break;
    case IROpcodeClass::ADD64: CompileADD64(context, lunatic_cast<IRAdd64>(op.get())); break;
   
    // Memory read/write (compile_memory.cpp)
    case IROpcodeClass::MemoryRead: CompileMemoryRead(context, lunatic_cast<IRMemoryRead>(op.get())); break;
    case IROpcodeClass::MemoryWrite: CompileMemoryWrite(context, lunatic_cast<IRMemoryWrite>(op.get())); break;
    
    // Pipeline flush (compile_flush.cpp)
    case IROpcodeClass::Flush: CompileFlush(context, lunatic_cast<IRFlush>(op.get())); break;
    case IROpcodeClass::FlushExchange: CompileFlushExchange(context, lunatic_cast<IRFlushExchange>(op.get())); break;

    // Coprocessor access (compile_coprocessor.cpp)
    case IROpcodeClass::MRC: CompileMRC(context, lunatic_cast<IRReadCoprocessorRegister>(op.get())); break;
    case IROpcodeClass::MCR: CompileMCR(context, lunatic_cast<IRWriteCoprocessorRegister>(op.get())); break;

    // SIMD (media instructions)
    case IROpcodeClass::SADD16: CompileSADD16(context, lunatic_cast<IRSignedAdd16>(op.get())); break;

    default: {
      throw std::runtime_error(
        fmt::format("lunatic: unhandled IR opcode: {}", op->ToString())
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

auto X64Backend::GetUsedHostRegsFromList(
  X64RegisterAllocator const& reg_alloc,
  std::vector<Xbyak::Reg64> const& regs
) -> std::vector<Xbyak::Reg64> {
  auto regs_used = std::vector<Xbyak::Reg64>{};

  for (auto reg : regs) {
    if (!reg_alloc.IsHostRegFree(reg)) {
      regs_used.push_back(reg);
    }
  }

  return regs_used;
}

} // namespace lunatic::backend
} // namespace lunatic
