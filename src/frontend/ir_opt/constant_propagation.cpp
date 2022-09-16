/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/bit.hpp"
#include "frontend/ir_opt/constant_propagation.hpp"

namespace lunatic {
namespace frontend {

void IRConstantPropagationPass::Run(IREmitter& emitter) {
  this->emitter = &emitter;
  var_to_const.clear();
  var_to_const.resize(emitter.Vars().size());

  for (auto& op : emitter.Code()) {
    switch (op->GetClass()) {
      case IROpcodeClass::MOV: DoMOV(op); break;
      case IROpcodeClass::LSL: DoLSL(op); break;
      case IROpcodeClass::LSR: DoLSR(op); break;
      case IROpcodeClass::ASR: DoASR(op); break;
      case IROpcodeClass::ROR: DoROR(op); break;
      case IROpcodeClass::ADD: DoBinaryOp<IRAdd>(op); break;
      case IROpcodeClass::SUB: DoBinaryOp<IRSub>(op); break;
      case IROpcodeClass::AND: DoBinaryOp<IRBitwiseAND>(op); break;
      case IROpcodeClass::BIC: DoBinaryOp<IRBitwiseBIC>(op); break;
      case IROpcodeClass::EOR: DoBinaryOp<IRBitwiseEOR>(op); break;
      case IROpcodeClass::ORR: DoBinaryOp<IRBitwiseORR>(op); break;
      case IROpcodeClass::MUL: DoMUL(op); break;
    }
  }
}

void IRConstantPropagationPass::Propagate(IRVariable const& var, IRConstant const& constant) {
  var_to_const[var.id] = constant;

  // TODO: start at the opcode where the variable is first written.
  for (auto& op : emitter->Code()) {
    if (op->Reads(var)) {
      op->PropagateConstant(var, constant);
    }
  }
};

auto IRConstantPropagationPass::GetKnownConstant(IRVarRef const& var) -> Optional<IRConstant>& {
  return var_to_const[var.Get().id];
};

void IRConstantPropagationPass::DoMOV(std::unique_ptr<IROpcode>& op) {
  auto mov_op = lunatic_cast<IRMov>(op.get());

  if (mov_op->source.IsConstant()) {
    Propagate(mov_op->result.Get(), mov_op->source.GetConst());
  }
}

void IRConstantPropagationPass::DoLSL(std::unique_ptr<IROpcode>& op) {
  auto lsl_op = lunatic_cast<IRLogicalShiftLeft>(op.get());

  auto& result = lsl_op->result.Get();
  auto& operand = GetKnownConstant(lsl_op->operand);
  auto& amount = lsl_op->amount;

  if (operand.HasValue() && amount.IsConstant()) {
    int shift_amount = (int)(amount.GetConst().value & 255);
    IRConstant constant = shift_amount >= 32 ? 0 : (operand.Unwrap().value << shift_amount);

    Propagate(result, constant);

    if (!lsl_op->update_host_flags) {
      op = MOV(result, constant);
    }
  }
}

void IRConstantPropagationPass::DoLSR(std::unique_ptr<IROpcode>& op) {
  auto lsr_op = lunatic_cast<IRLogicalShiftRight>(op.get());

  auto& result = lsr_op->result.Get();
  auto& operand = GetKnownConstant(lsr_op->operand);
  auto& amount = lsr_op->amount;

  if (operand.HasValue() && amount.IsConstant()) {
    int shift_amount = (int)(amount.GetConst().value & 255);
    IRConstant constant = (shift_amount == 0 || shift_amount >= 32) ? 0 : (operand.Unwrap().value >> shift_amount);

    Propagate(result, constant);

    if (!lsr_op->update_host_flags) {
      op = MOV(result, constant);
    }
  }
}

void IRConstantPropagationPass::DoASR(std::unique_ptr<IROpcode>& op) {
  auto asr_op = lunatic_cast<IRArithmeticShiftRight>(op.get());

  auto& result = asr_op->result.Get();
  auto& operand = GetKnownConstant(asr_op->operand);
  auto& amount = asr_op->amount;

  if (operand.HasValue() && amount.IsConstant()) {
    int shift_amount = (int)(amount.GetConst().value & 255);
    u32 operand_value = operand.Unwrap().value;

    if (shift_amount == 0 || shift_amount >= 32) {
      shift_amount = 31;
    }

    IRConstant constant = (u32)((s32)operand_value >> shift_amount);

    Propagate(result, constant);

    if (!asr_op->update_host_flags) {
      op = MOV(result, constant);
    }
  }
}

void IRConstantPropagationPass::DoROR(std::unique_ptr<IROpcode>& op) {
  auto ror_op = lunatic_cast<IRRotateRight>(op.get());

  auto& result = ror_op->result.Get();
  auto& operand = GetKnownConstant(ror_op->operand);
  auto& amount = ror_op->amount;

  if (operand.HasValue() && amount.IsConstant()) {
    u32 shift_amount = amount.GetConst().value;

    if (shift_amount == 0) {
      // RRX #1
      return;
    }

    IRConstant constant = bit::rotate_right(operand.Unwrap().value, shift_amount & 31);

    Propagate(ror_op->result.Get(), constant);

    if (!ror_op->update_host_flags) {
      op = MOV(result, constant);
    }
  }
}

template<typename OpcodeType>
void IRConstantPropagationPass::DoBinaryOp(std::unique_ptr<IROpcode>& op) {
  constexpr IROpcodeClass klass = OpcodeType::klass;

  auto bin_op = lunatic_cast<OpcodeType>(op.get());
  auto& result = bin_op->result;
  auto& lhs = GetKnownConstant(bin_op->lhs);
  auto& rhs = bin_op->rhs;

  if (lhs.HasValue() && rhs.IsConstant()) {
    IRConstant constant;

    u32 lhs_value = lhs.Unwrap().value;
    u32 rhs_value = rhs.GetConst().value;

    switch (klass) {
      case IROpcodeClass::ADD: constant = lhs_value +  rhs_value; break;
      case IROpcodeClass::SUB: constant = lhs_value -  rhs_value; break;
      case IROpcodeClass::AND: constant = lhs_value &  rhs_value; break;
      case IROpcodeClass::BIC: constant = lhs_value & ~rhs_value; break;
      case IROpcodeClass::EOR: constant = lhs_value ^  rhs_value; break;
      case IROpcodeClass::ORR: constant = lhs_value |  rhs_value; break;
    }

    if (result.HasValue()) {
      Propagate(result.Unwrap(), constant);
    }

    bool update_host_flags = bin_op->update_host_flags;

    // Attempt to replace opcode with a MOV (removes dependencies on operand variables)
    if (result.HasValue()) {
      if (klass == IROpcodeClass::ADD || klass == IROpcodeClass::SUB) {
        if (!update_host_flags) {
          op = MOV(result.Unwrap(), constant, false);
        }
      } else {
        // AND, BIC, EOR, ORR
        op = MOV(result.Unwrap(), constant, update_host_flags);
      }
    } else if (!update_host_flags) {
      op = NOP();
    }
  }
}

void IRConstantPropagationPass::DoMUL(std::unique_ptr<IROpcode>& op) {
  const auto mul_op = (IRMultiply*)op.get();

  auto& lhs = GetKnownConstant(mul_op->lhs.Get());
  auto& rhs = GetKnownConstant(mul_op->rhs.Get());

  if (lhs.HasValue() && rhs.HasValue()) {
    if (mul_op->result_hi.HasValue()) {
      if (mul_op->lhs.Get().data_type == IRDataType::SInt32) {
        s64 result = (s64)(s32)lhs.Unwrap().value * (s64)(s32)rhs.Unwrap().value;
        IRConstant constant_lo = (u32)result;
        IRConstant constant_hi = (u32)(result >> 32);

        Propagate(mul_op->result_lo.Get(), constant_lo);
        Propagate(mul_op->result_hi.Unwrap(), constant_hi);
      } else {
        u64 result = (u64)lhs.Unwrap().value * (u64)rhs.Unwrap().value;
        IRConstant constant_lo = (u32)result;
        IRConstant constant_hi = (u32)(result >> 32);

        Propagate(mul_op->result_lo.Get(), constant_lo);
        Propagate(mul_op->result_hi.Unwrap(), constant_hi);
      }
    } else {
      IRConstant constant = lhs.Unwrap().value * rhs.Unwrap().value;

      Propagate(mul_op->result_lo.Get(), constant);
      op = MOV(mul_op->result_lo.Get(), constant, mul_op->update_host_flags);
    }
  }
}

} // namespace lunatic::frontend
} // namespace lunatic
