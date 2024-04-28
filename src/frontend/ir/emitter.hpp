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

namespace lunatic::frontend {

  class IREmitter {
    public:
      using OpcodePtr = std::unique_ptr<IROpcode>;
      using InstructionList = std::list<OpcodePtr, StdPoolAlloc<OpcodePtr>>;
      using VariableList = std::vector<std::unique_ptr<IRVariable>>;

      IREmitter() = default;
      IREmitter(const IREmitter&) = delete;
      IREmitter& operator=(const IREmitter&) = delete;

      IREmitter(IREmitter&& emitter) noexcept {
        operator=(std::move(emitter));
      }

      IREmitter& operator=(IREmitter&& emitter) {
        std::swap(m_code, emitter.m_code);
        std::swap(m_variables, emitter.m_variables);
        return *this;
      }

      InstructionList& Code() {
        return m_code;
      }

      [[nodiscard]] const InstructionList& Code() const {
        return m_code;
      }

      [[nodiscard]] const VariableList& Vars() const {
        return m_variables;
      }

      [[nodiscard]] std::string ToString() const;

      const IRVariable& CreateVar(IRDataType data_type, char const* label = nullptr);

      /// Context Load-Store Operations

      void LoadGPR(IRGuestReg reg, const IRVariable& result);
      void StoreGPR(IRGuestReg reg, IRAnyRef value);
      void LoadSPSR(const IRVariable& result, Mode mode);
      void StoreSPSR(IRAnyRef value, Mode mode);
      void LoadCPSR(const IRVariable& result);
      void StoreCPSR(IRAnyRef value);

      /// CPU Flag Operations

      void ClearCarry();
      void SetCarry();
      void UpdateNZ(const IRVariable& result, const IRVariable& input);
      void UpdateNZC(const IRVariable& result, const IRVariable& input);
      void UpdateNZCV(const IRVariable& result, const IRVariable& input);
      void UpdateQ(const IRVariable& result, const IRVariable& input);

      /// Shifter Operations

      void LSL(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags);
      void LSR(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags);
      void ASR(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags);
      void ROR(const IRVariable& result, const IRVariable& operand, IRAnyRef amount, bool update_host_flags);

      /// ALU Operations

      void AND(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void BIC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void EOR(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void SUB(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void RSB(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void ADD(Optional<const IRVariable&> result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void ADC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void SBC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void RSC(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void ORR(const IRVariable& result, const IRVariable& lhs, IRAnyRef rhs, bool update_host_flags);
      void MOV(const IRVariable& result, IRAnyRef source, bool update_host_flags);
      void MVN(const IRVariable& result, IRAnyRef source, bool update_host_flags);
      void CLZ(const IRVariable& result, const IRVariable& operand);
      void QADD(const IRVariable& result, const IRVariable& lhs, const IRVariable& rhs);
      void QSUB(const IRVariable& result, const IRVariable& lhs, const IRVariable& rhs);

      /// Multiplier Operations

      void MUL(
        Optional<const IRVariable&> result_hi,
        const IRVariable& result_lo,
        const IRVariable& lhs,
        const IRVariable& rhs,
        bool update_host_flags
      );

      void ADD64(
        const IRVariable& result_hi,
        const IRVariable& result_lo,
        const IRVariable& lhs_hi,
        const IRVariable& lhs_lo,
        const IRVariable& rhs_hi,
        const IRVariable& rhs_lo,
        bool update_host_flags
      );

      /// Memory Read/Write Operations

      void LDR(IRMemoryFlags flags, const IRVariable& result, const IRVariable& address);
      void STR(IRMemoryFlags flags, const IRVariable& source, const IRVariable& address);

      /// Pipeline reload and ARM/Thumb switch operations

      void Flush(const IRVariable& address_out, const IRVariable& address_in, const IRVariable& cpsr_in);
      void FlushExchange(const IRVariable& address_out, const IRVariable& cpsr_out, const IRVariable& address_in, const IRVariable& cpsr_in);

      /// Coprocessor Operations

      void MRC(const IRVariable& result, int coprocessor_id, int opcode1, int cn, int cm, int opcode2);
      void MCR(IRAnyRef value, int coprocessor_id, int opcode1, int cn, int cm, int opcode2);

    private:
      template<typename T, typename... Args>
      void Push(Args&&... args) {
        m_code.push_back(std::make_unique<T>(args...));
      }

      InstructionList m_code{};
      VariableList m_variables{};
  };

} // namespace lunatic::frontend
