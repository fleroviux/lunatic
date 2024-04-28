/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdexcept>

#include "emitter.hpp"

namespace lunatic::frontend {

  std::string IREmitter::ToString() const {
    std::string source;
    size_t location = 0;

    for (const auto& var : m_variables) {
      source += fmt::format("{} {}\r\n", std::to_string(var->data_type), std::to_string(*var));
    }

    source += "\r\n";

    for (const auto& op : m_code) {
      source += fmt::format("{:03} {}\r\n", location++, op->ToString());
    }

    return source;
  }

  const IRVariable& IREmitter::CreateVar(IRDataType data_type, char const* label) {
    auto id = (u32)m_variables.size();
    auto var = new IRVariable{id, data_type, label};

    m_variables.push_back(std::unique_ptr<IRVariable>(var));
    return *var;
  }

  void IREmitter::LoadGPR(IRGuestReg reg, const IRVariable& result) {
    Push<IRLoadGPR>(reg, result);
  }

  void IREmitter::StoreGPR(IRGuestReg reg, IRAnyRef value) {
    if (value.IsNull()) {
      throw std::runtime_error("StoreGPR: value must not be null");
    }
    Push<IRStoreGPR>(reg, value);
  }

  void IREmitter::LoadSPSR (const IRVariable& result, Mode mode) {
    if (mode == Mode::User || mode == Mode::System) {
      Push<IRLoadCPSR>(result);
    } else {
      Push<IRLoadSPSR>(result, mode);
    }
  }

  void IREmitter::StoreSPSR(IRAnyRef value, Mode mode) {
    if (mode == Mode::User || mode == Mode::System) {
      return;
    }
    Push<IRStoreSPSR>(value, mode);
  }

  void IREmitter::LoadCPSR(const IRVariable& result) {
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

  void IREmitter::UpdateNZ(const IRVariable& result, const IRVariable& input) {
    Push<IRUpdateFlags>(result, input, true, true, false, false);
  }

  void IREmitter::UpdateNZC(const IRVariable& result, const IRVariable& input) {
    Push<IRUpdateFlags>(result, input, true, true, true, false);
  }

  void IREmitter::UpdateNZCV(const IRVariable& result, const IRVariable& input) {
    Push<IRUpdateFlags>(result, input, true, true, true, true);
  }

  void IREmitter::UpdateQ(const IRVariable& result, const IRVariable& input) {
    Push<IRUpdateSticky>(result, input);
  }

  void IREmitter::LSL(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags) {
    if (amount.IsNull()) {
      throw std::runtime_error("LSL: amount must not be null");
    }
    Push<IRLogicalShiftLeft>(result, operand, amount, update_host_flags);
  }

  void IREmitter::LSR(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags) {
    if (amount.IsNull()) {
      throw std::runtime_error("LSR: amount must not be null");
    }
    Push<IRLogicalShiftRight>(result, operand, amount, update_host_flags);
  }

  void IREmitter::ASR(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags) {
    if (amount.IsNull()) {
      throw std::runtime_error("ASR: amount must not be null");
    }
    Push<IRArithmeticShiftRight>(result, operand, amount, update_host_flags);
  }

  void IREmitter::ROR(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags) {
    if (amount.IsNull()) {
      throw std::runtime_error("ROR: amount must not be null");
    }
    Push<IRRotateRight>(result, operand, amount, update_host_flags);
  }

  void IREmitter::AND(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("AND: rhs operand must not be null");
    }
    Push<IRBitwiseAND>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::BIC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("BIC: rhs operand must not be null");
    }
    Push<IRBitwiseBIC>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::EOR(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("EOR: rhs operand must not be null");
    }
    Push<IRBitwiseEOR>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::SUB(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("SUB: rhs operand must not be null");
    }
    Push<IRSub>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::RSB(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("RSB: rhs operand must not be null");
    }
    Push<IRRsb>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::ADD(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("ADD: rhs operand must not be null");
    }
    Push<IRAdd>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::ADC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("ADC: rhs operand must not be null");
    }
    Push<IRAdc>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::SBC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("SBC: rhs operand must not be null");
    }
    Push<IRSbc>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::RSC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("RSC: rhs operand must not be null");
    }
    Push<IRRsc>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::ORR(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags) {
    if (rhs.IsNull()) {
      throw std::runtime_error("ORR: rhs operand must not be null");
    }
    Push<IRBitwiseORR>(result, lhs, rhs, update_host_flags);
  }

  void IREmitter::MOV(const IRVariable& result, IRAnyRef source, bool update_host_flags) {
    Push<IRMov>(result, source, update_host_flags);
  }

  void IREmitter::MVN(const IRVariable& result, IRAnyRef source, bool update_host_flags) {
    Push<IRMvn>(result, source, update_host_flags);
  }

  void IREmitter::CLZ(const IRVariable& result, const IRVariable& operand) {
    Push<IRCountLeadingZeros>(result, operand);
  }

  void IREmitter::QADD(const IRVariable& result, const IRVariable& lhs, const IRVariable& rhs) {
    Push<IRSaturatingAdd>(result, lhs, rhs);
  }

  void IREmitter::QSUB(const IRVariable& result, const IRVariable& lhs, const IRVariable& rhs) {
    Push<IRSaturatingSub>(result, lhs, rhs);
  }

  void IREmitter::MUL(
    Optional<const IRVariable&> result_hi,
    const IRVariable& result_lo,
    const IRVariable& lhs,
    const IRVariable& rhs,
    bool update_host_flags
  ) {
    if (lhs.data_type != rhs.data_type) {
      throw std::runtime_error("MUL: LHS and RHS operands must have same data type.");
    }
    Push<IRMultiply>(result_hi, result_lo, lhs, rhs, update_host_flags);
  }

  void IREmitter::ADD64(
    const IRVariable& result_hi,
    const IRVariable& result_lo,
    const IRVariable& lhs_hi,
    const IRVariable& lhs_lo,
    const IRVariable& rhs_hi,
    const IRVariable& rhs_lo,
    bool update_host_flags
  ) {
    Push<IRAdd64>(result_hi, result_lo, lhs_hi, lhs_lo, rhs_hi, rhs_lo, update_host_flags);
  }

  void IREmitter::LDR(IRMemoryFlags flags, const IRVariable& result, const IRVariable& address) {
    Push<IRMemoryRead>(flags, result, address);
  }

  void IREmitter::STR(IRMemoryFlags flags, const IRVariable& source, const IRVariable& address) {
    Push<IRMemoryWrite>(flags, source, address);
  }

  void IREmitter::Flush(const IRVariable& address_out, const IRVariable& address_in, const IRVariable& cpsr_in) {
    Push<IRFlush>(address_out, address_in, cpsr_in);
  }

  void IREmitter::FlushExchange(const IRVariable& address_out, const IRVariable& cpsr_out, const IRVariable& address_in, const IRVariable& cpsr_in) {
    Push<IRFlushExchange>(address_out, cpsr_out, address_in, cpsr_in);
  }

  void IREmitter::MRC(const IRVariable& result, int coprocessor_id, int opcode1, int cn, int cm, int opcode2) {
    Push<IRReadCoprocessorRegister>(result, coprocessor_id, opcode1, cn, cm, opcode2);
  }

  void IREmitter::MCR(IRAnyRef value, int coprocessor_id, int opcode1, int cn, int cm, int opcode2) {
    Push<IRWriteCoprocessorRegister>(value, coprocessor_id, opcode1, cn, cm, opcode2);
  }

} // namespace lunatic::frontend
