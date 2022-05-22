/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <list>
#include <memory>
#include <vector>

#include "common/optional.hpp"
#include "common/pool_allocator.hpp"
#include "opcode.hpp"

namespace lunatic {
namespace frontend {

struct IREmitter {
  using OpcodePtr = std::unique_ptr<IROpcode>;
  using InstructionList = std::list<OpcodePtr, StdPoolAlloc<OpcodePtr>>;
  using VariableList = std::vector<std::unique_ptr<IRVariable>>;

  IREmitter() = default;
  IREmitter(const IREmitter&) = delete;
  IREmitter& operator=(const IREmitter&) = delete;

  IREmitter(IREmitter&& emitter) {
    operator=(std::move(emitter));
  }

  IREmitter& operator=(IREmitter&& emitter) {
    std::swap(code, emitter.code);
    std::swap(variables, emitter.variables);
    return *this;
  }

  auto Code() -> InstructionList& { return code; }
  auto Code() const -> InstructionList const& { return code; }
  auto Vars() const -> VariableList const& { return variables; }
  auto ToString() const -> std::string;

  auto CreateVar(
    IRDataType data_type,
    char const* label = nullptr
  ) -> IRVariable const&;

  void LoadGPR (IRGuestReg reg, IRVariable const& result);
  void StoreGPR(IRGuestReg reg, IRAnyRef value);

  void LoadSPSR (IRVariable const& result, Mode mode);
  void StoreSPSR(IRAnyRef value, Mode mode);
  void LoadCPSR (IRVariable const& result);
  void StoreCPSR(IRAnyRef value);

  void ClearCarry();
  void SetCarry();

  void UpdateNZ(
    IRVariable const& result,
    IRVariable const& input
  );

  void UpdateNZC(
    IRVariable const& result,
    IRVariable const& input
  );

  void UpdateNZCV(
    IRVariable const& result,
    IRVariable const& input
  );

  void UpdateQ(
    IRVariable const& result,
    IRVariable const& input
  );

  void UpdateGE(
    IRVariable const& result,
    IRVariable const& input
  );

  void LSL(
    IRVariable const& result,
    IRVariable const& operand,
    IRAnyRef amount,
    bool update_host_flags
  );

  void LSR(
    IRVariable const& result,
    IRVariable const& operand,
    IRAnyRef amount,
    bool update_host_flags
  );

  void ASR(
    IRVariable const& result,
    IRVariable const& operand,
    IRAnyRef amount,
    bool update_host_flags
  );

  void ROR(
    IRVariable const& result,
    IRVariable const& operand,
    IRAnyRef amount,
    bool update_host_flags
  );

  void AND(
    Optional<IRVariable const&> result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void BIC(
    IRVariable const& result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void EOR(
    Optional<IRVariable const&> result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void SUB(
    Optional<IRVariable const&> result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void RSB(
    IRVariable const& result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void ADD(
    Optional<IRVariable const&> result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void ADC(
    IRVariable const& result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void SBC(
    IRVariable const& result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void RSC(
    IRVariable const& result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void ORR(
    IRVariable const& result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  );

  void MOV(
    IRVariable const& result,
    IRAnyRef source,
    bool update_host_flags
  );

  void MVN(
    IRVariable const& result,
    IRAnyRef source,
    bool update_host_flags
  );

  void MUL(
    Optional<IRVariable const&> result_hi,
    IRVariable const& result_lo,
    IRVariable const& lhs,
    IRVariable const& rhs,
    bool update_host_flags
  );

  void ADD64(
    IRVariable const& result_hi,
    IRVariable const& result_lo,
    IRVariable const& lhs_hi,
    IRVariable const& lhs_lo,
    IRVariable const& rhs_hi,
    IRVariable const& rhs_lo,
    bool update_host_flags
  );

  void LDR(
    IRMemoryFlags flags,
    IRVariable const& result,
    IRVariable const& address
  );

  void STR(
    IRMemoryFlags flags,
    IRVariable const& source,
    IRVariable const& address
  );

  void Flush(
    IRVariable const& address_out,
    IRVariable const& address_in,
    IRVariable const& cpsr_in
  );

  void FlushExchange(
    IRVariable const& address_out,
    IRVariable const& cpsr_out,
    IRVariable const& address_in,
    IRVariable const& cpsr_in
  );

  void CLZ(
    IRVariable const& result,
    IRVariable const& operand
  );

  void QADD(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void QSUB(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void MRC(
    IRVariable const& result,
    int coprocessor_id,
    int opcode1,
    int cn,
    int cm,
    int opcode2
  );

  void MCR(
    IRAnyRef value,
    int coprocessor_id,
    int opcode1,
    int cn,
    int cm,
    int opcode2
  );

  void PADDS8(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PADDS16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PADDU16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PSUBS16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PSUBU16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PQADDS16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PQADDU16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PQSUBS16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PQSUBU16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PHADDS16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PHADDU16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PHSUBS16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

  void PHSUBU16(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  );

private:
  template<typename T, typename... Args>
  void Push(Args&&... args) {
    code.push_back(std::make_unique<T>(args...));
  }

  InstructionList code;
  VariableList variables;
};

} // namespace lunatic::frontend
} // namespace lunatic
