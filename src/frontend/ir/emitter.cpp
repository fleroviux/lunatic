/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdexcept>

#include "emitter.hpp"

namespace lunatic {
namespace frontend {

auto IREmitter::ToString() const -> std::string {
  std::string source;
  size_t location = 0;

  for (auto const& var : variables) {
    source += fmt::format("{} {}\r\n", std::to_string(var->data_type), std::to_string(*var));
  }

  source += "\r\n";

  for (auto const& op : code) {
    source += fmt::format("{:03} {}\r\n", location++, op->ToString());
  }

  return source;
}

auto IREmitter::CreateVar(
  IRDataType data_type,
  char const* label
) -> IRVariable const& {
  auto id = u32(variables.size());
  auto var = new IRVariable{id, data_type, label};

  variables.push_back(std::unique_ptr<IRVariable>(var));
  return *var;
}

void IREmitter::LoadGPR(IRGuestReg reg, IRVariable const& result) {
  Push<IRLoadGPR>(reg, result);
}

void IREmitter::StoreGPR(IRGuestReg reg, IRAnyRef value) {
  if (value.IsNull()) {
    throw std::runtime_error("StoreGPR: value must not be null");
  }
  Push<IRStoreGPR>(reg, value);
}

void IREmitter::LoadSPSR (IRVariable const& result, Mode mode) {
  // TODO: I'm not sure if here is the right place to handle this.
  if (mode == Mode::User || mode == Mode::System) {
    Push<IRLoadCPSR>(result);
  } else {
    Push<IRLoadSPSR>(result, mode);
  }
}

void IREmitter::StoreSPSR(IRAnyRef value, Mode mode) {
  // TODO: I'm not sure if here is the right place to handle this.
  if (mode == Mode::User || mode == Mode::System) {
    return;
  }
  Push<IRStoreSPSR>(value, mode);
}

void IREmitter::LoadCPSR(IRVariable const& result) {
  Push<IRLoadCPSR>(result);
}

void IREmitter::StoreCPSR(IRAnyRef value) {
  if (value.IsNull()) {
    throw std::runtime_error("StoreCPSR: value must not be null");
  }
  Push<IRStoreCPSR>(value);
}

void IREmitter::ClearCarry() {
  Push<IRClearCarry>();
}

void IREmitter::SetCarry() {
  Push<IRSetCarry>();
}

void IREmitter::UpdateNZ(
  IRVariable const& result,
  IRVariable const& input
) {
  Push<IRUpdateFlags>(result, input, true, true, false, false);
}

void IREmitter::UpdateNZC(
  IRVariable const& result,
  IRVariable const& input
) {
  Push<IRUpdateFlags>(result, input, true, true, true, false);
}

void IREmitter::UpdateNZCV(
  IRVariable const& result,
  IRVariable const& input
) {
  Push<IRUpdateFlags>(result, input, true, true, true, true);
}

void IREmitter::UpdateQ(
  IRVariable const& result,
  IRVariable const& input
) {
  Push<IRUpdateSticky>(result, input);
}

void IREmitter::UpdateGE(
  IRVariable const& result,
  IRVariable const& input
) {
  Push<IRUpdateGE>(result, input);
}

void IREmitter::LSL(
  IRVariable const& result,
  IRVariable const& operand,
  IRAnyRef amount,
  bool update_host_flags
) {
  if (amount.IsNull()) {
    throw std::runtime_error("LSL: amount must not be null");
  }
  Push<IRLogicalShiftLeft>(result, operand, amount, update_host_flags);
}

void IREmitter::LSR(
  IRVariable const& result,
  IRVariable const& operand,
  IRAnyRef amount,
  bool update_host_flags
) {
  if (amount.IsNull()) {
    throw std::runtime_error("LSR: amount must not be null");
  }
  Push<IRLogicalShiftRight>(result, operand, amount, update_host_flags);
}

void IREmitter::ASR(
  IRVariable const& result,
  IRVariable const& operand,
  IRAnyRef amount,
  bool update_host_flags
) {
  if (amount.IsNull()) {
    throw std::runtime_error("ASR: amount must not be null");
  }
  Push<IRArithmeticShiftRight>(result, operand, amount, update_host_flags);
}

void IREmitter::ROR(
  IRVariable const& result,
  IRVariable const& operand,
  IRAnyRef amount,
  bool update_host_flags
) {
  if (amount.IsNull()) {
    throw std::runtime_error("ROR: amount must not be null");
  }
  Push<IRRotateRight>(result, operand, amount, update_host_flags);
}

void IREmitter::AND(
  Optional<IRVariable const&> result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("AND: rhs operand must not be null");
  }
  Push<IRBitwiseAND>(result, lhs, rhs, update_host_flags);
}

void IREmitter::BIC(
  IRVariable const& result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("BIC: rhs operand must not be null");
  }
  Push<IRBitwiseBIC>(result, lhs, rhs, update_host_flags);
}

void IREmitter::EOR(
  Optional<IRVariable const&> result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("EOR: rhs operand must not be null");
  }
  Push<IRBitwiseEOR>(result, lhs, rhs, update_host_flags);
}

void IREmitter::SUB(
  Optional<IRVariable const&> result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("SUB: rhs operand must not be null");
  }
  Push<IRSub>(result, lhs, rhs, update_host_flags);
}

void IREmitter::RSB(
  IRVariable const& result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("RSB: rhs operand must not be null");
  }
  Push<IRRsb>(result, lhs, rhs, update_host_flags);
}

void IREmitter::ADD(
  Optional<IRVariable const&> result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("ADD: rhs operand must not be null");
  }
  Push<IRAdd>(result, lhs, rhs, update_host_flags);
}

void IREmitter::ADC(
  IRVariable const& result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("ADC: rhs operand must not be null");
  }
  Push<IRAdc>(result, lhs, rhs, update_host_flags);
}

void IREmitter::SBC(
  IRVariable const& result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("SBC: rhs operand must not be null");
  }
  Push<IRSbc>(result, lhs, rhs, update_host_flags);
}

void IREmitter::RSC(
  IRVariable const& result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("RSC: rhs operand must not be null");
  }
  Push<IRRsc>(result, lhs, rhs, update_host_flags);
}

void IREmitter::ORR(
  IRVariable const& result,
  IRVariable const& lhs,
  IRAnyRef rhs,
  bool update_host_flags
) {
  if (rhs.IsNull()) {
    throw std::runtime_error("ORR: rhs operand must not be null");
  }
  Push<IRBitwiseORR>(result, lhs, rhs, update_host_flags);
}

void IREmitter::MOV(
  IRVariable const& result,
  IRAnyRef source,
  bool update_host_flags
) {
  Push<IRMov>(result, source, update_host_flags);
}

void IREmitter::MVN(
  IRVariable const& result,
  IRAnyRef source,
  bool update_host_flags
) {
  Push<IRMvn>(result, source, update_host_flags);
}

void IREmitter::MUL(
  Optional<IRVariable const&> result_hi,
  IRVariable const& result_lo,
  IRVariable const& lhs,
  IRVariable const& rhs,
  bool update_host_flags
) {
  if (lhs.data_type != rhs.data_type) {
    throw std::runtime_error("MUL: LHS and RHS operands must have same data type.");
  }
  Push<IRMultiply>(result_hi, result_lo, lhs, rhs, update_host_flags);
}

void IREmitter::ADD64(
  IRVariable const& result_hi,
  IRVariable const& result_lo,
  IRVariable const& lhs_hi,
  IRVariable const& lhs_lo,
  IRVariable const& rhs_hi,
  IRVariable const& rhs_lo,
  bool update_host_flags
) {
  Push<IRAdd64>(result_hi, result_lo, lhs_hi, lhs_lo, rhs_hi, rhs_lo, update_host_flags);
}

void IREmitter::LDR(
  IRMemoryFlags flags,
  IRVariable const& result,
  IRVariable const& address
) {
  Push<IRMemoryRead>(flags, result, address);
}

void IREmitter::STR(
  IRMemoryFlags flags,
  IRVariable const& source,
  IRVariable const& address
) {
  Push<IRMemoryWrite>(flags, source, address);
}

void IREmitter::Flush(
  IRVariable const& address_out,
  IRVariable const& address_in,
  IRVariable const& cpsr_in
) {
  Push<IRFlush>(address_out, address_in, cpsr_in);
}

void IREmitter::FlushExchange(
  IRVariable const& address_out,
  IRVariable const& cpsr_out,
  IRVariable const& address_in,
  IRVariable const& cpsr_in
) {
  Push<IRFlushExchange>(address_out, cpsr_out, address_in, cpsr_in);
}

void IREmitter::CLZ(
  IRVariable const& result,
  IRVariable const& operand
) {
  Push<IRCountLeadingZeros>(result, operand);
}

void IREmitter::QADD(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRSaturatingAdd>(result, lhs, rhs);
}

void IREmitter::QSUB(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRSaturatingSub>(result, lhs, rhs);
}

void IREmitter::MRC(
  IRVariable const& result,
  int coprocessor_id,
  int opcode1,
  int cn,
  int cm,
  int opcode2
) {
  Push<IRReadCoprocessorRegister>(result, coprocessor_id, opcode1, cn, cm, opcode2);
}

void IREmitter::MCR(
  IRAnyRef value,
  int coprocessor_id,
  int opcode1,
  int cn,
  int cm,
  int opcode2
) {
  Push<IRWriteCoprocessorRegister>(value, coprocessor_id, opcode1, cn, cm, opcode2);
}

void IREmitter::PADDS8(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelAddS8>(result, lhs, rhs);
}

void IREmitter::PADDU8(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelAddU8>(result, lhs, rhs);
}

void IREmitter::PADDS16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelAddS16>(result, lhs, rhs);
}

void IREmitter::PADDU16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelAddU16>(result, lhs, rhs);
}

void IREmitter::PSUBS16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSubS16>(result, lhs, rhs);
}

void IREmitter::PSUBU16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSubU16>(result, lhs, rhs);
}

void IREmitter::PQADDS8(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSaturateAddS8>(result, lhs, rhs);
}

void IREmitter::PQADDU8(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSaturateAddU8>(result, lhs, rhs);
}

void IREmitter::PQADDS16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSaturateAddS16>(result, lhs, rhs);
}

void IREmitter::PQADDU16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSaturateAddU16>(result, lhs, rhs);
}

void IREmitter::PQSUBS16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSaturateSubS16>(result, lhs, rhs);
}

void IREmitter::PQSUBU16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelSaturateSubU16>(result, lhs, rhs);
}

void IREmitter::PHADDS8(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelHalvingAddS8>(result, lhs, rhs);
}

void IREmitter::PHADDU8(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelHalvingAddU8>(result, lhs, rhs);
}

void IREmitter::PHADDS16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelHalvingAddS16>(result, lhs, rhs);
}

void IREmitter::PHADDU16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelHalvingAddU16>(result, lhs, rhs);
}

void IREmitter::PHSUBS16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelHalvingSubS16>(result, lhs, rhs);
}

void IREmitter::PHSUBU16(
  IRVariable const& result,
  IRVariable const& lhs,
  IRVariable const& rhs
) {
  Push<IRParallelHalvingSubU16>(result, lhs, rhs);
}

} // namespace lunatic::frontend
} // namespace lunatic
