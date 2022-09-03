/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <vector>

#include "common/bit.hpp"
#include "frontend/ir_opt/constant_propagation.hpp"

namespace lunatic {
namespace frontend {

void IRConstantPropagationPass::Run(IREmitter& emitter) {
  const auto p = [&](std::unique_ptr<IROpcode> const& op) {
    fmt::print("propagate: {}\n", op->ToString());
  };

  std::vector<Optional<IRConstant>> var_to_const{};

  var_to_const.resize(emitter.Vars().size());

  const auto Propagate = [&](IRVariable const& var, IRConstant const& constant) {
    var_to_const[var.id] = constant;

    // TODO: start at the opcode where the variable is first written.
    for (auto& op : emitter.Code()) {
      if (op->Reads(var)) {
        fmt::print("propagated to: {}\n", op->ToString());
        op->PropagateConstant(var, constant);
      }
    }
  };

  const auto GetKnownConstant = [&](IRVarRef const& var)-> Optional<IRConstant>& {
    return var_to_const[var.Get().id];
  };

  const auto MOV = [](IRVariable const& result, IRConstant const& constant, bool update_host_flags = false) {
    return std::make_unique<IRMov>(result, constant, update_host_flags);
  };

  const auto NOP = []() {
    return std::make_unique<IRNoOp>();
  };

  for (auto& op : emitter.Code()) {
    const auto op_class = op->GetClass();

    switch (op_class) {
      case IROpcodeClass::MOV: {
        const auto mov = (IRMov*)op.get();

        // TODO: this breaks the MOV test in ARMWrestler somehow; games appear to work fine though.
        if (mov->source.IsConstant()) {
          p(op);
          Propagate(mov->result.Get(), mov->source.GetConst());
        }
        break;
      }
      case IROpcodeClass::LSL: {
        const auto lsl = (IRLogicalShiftLeft*)op.get();

        auto& result = lsl->result.Get();
        auto& operand = GetKnownConstant(lsl->operand);
        auto& amount = lsl->amount;

        if (operand.HasValue() && amount.IsConstant()) {
          int shift_amount = amount.GetConst().value & 255;
          IRConstant constant = shift_amount >= 32 ? 0 : (operand.Unwrap().value << shift_amount);

          p(op);
          Propagate(result, constant);

          if (!lsl->update_host_flags) {
            op = MOV(result, constant);
          }
        }
        break;
      }
      case IROpcodeClass::LSR: {
        const auto lsr = (IRLogicalShiftRight*)op.get();

        auto& result = lsr->result.Get();
        auto& operand = GetKnownConstant(lsr->operand);
        auto& amount = lsr->amount;

        if (operand.HasValue() && amount.IsConstant()) {
          int shift_amount = amount.GetConst().value & 255;
          IRConstant constant = (shift_amount == 0 || shift_amount >= 32) ? 0 : (operand.Unwrap().value >> shift_amount);

          p(op);
          Propagate(result, constant);

          if (!lsr->update_host_flags) {
            op = MOV(result, constant);
          }
        }
        break;
      }
      case IROpcodeClass::ASR: {
        const auto asr = (IRArithmeticShiftRight*)op.get();

        auto& result = asr->result.Get();
        auto& operand = GetKnownConstant(asr->operand);
        auto& amount = asr->amount;

        if (operand.HasValue() && amount.IsConstant()) {
          int shift_amount = amount.GetConst().value & 255;
          u32 operand_value = operand.Unwrap().value;

          if (shift_amount == 0 || shift_amount >= 32) {
            shift_amount = 31;
          }

          IRConstant constant = (u32)((s32)operand_value >> shift_amount);

          p(op);
          Propagate(result, constant);

          if (!asr->update_host_flags) {
            op = MOV(result, constant);
          }
        }
        break;
      }
      case IROpcodeClass::ROR: {
        const auto ror = (IRRotateRight*)op.get();

        auto& result = ror->result.Get();
        auto& operand = GetKnownConstant(ror->operand);
        auto& amount = ror->amount;

        if (operand.HasValue() && amount.IsConstant()) {
          int shift_amount = amount.GetConst().value;

          if (shift_amount == 0) {
            // RRX #1
            break;
          }

          IRConstant constant = bit::rotate_right(operand.Unwrap().value, shift_amount & 31);

          p(op);
          Propagate(ror->result.Get(), constant);

          if (!ror->update_host_flags) {
            op = MOV(result, constant);
          }
        }
        break;
      }
      case IROpcodeClass::ADD:
      case IROpcodeClass::SUB:
      case IROpcodeClass::AND:
      case IROpcodeClass::BIC:
      case IROpcodeClass::EOR:
      case IROpcodeClass::ORR: {
        const auto add = (IRAdd*)op.get(); // TODO: not always correct

        auto& result = add->result;
        auto& lhs = GetKnownConstant(add->lhs);
        auto& rhs = add->rhs;

        if (lhs.HasValue() && rhs.IsConstant()) {
          IRConstant constant;

          u32 lhs_value = lhs.Unwrap().value;
          u32 rhs_value = rhs.GetConst().value;

          switch (op_class) {
            case IROpcodeClass::ADD: constant = lhs_value +  rhs_value; break;
            case IROpcodeClass::SUB: constant = lhs_value -  rhs_value; break;
            case IROpcodeClass::AND: constant = lhs_value &  rhs_value; break;
            case IROpcodeClass::BIC: constant = lhs_value & ~rhs_value; break;
            case IROpcodeClass::EOR: constant = lhs_value ^  rhs_value; break;
            case IROpcodeClass::ORR: constant = lhs_value |  rhs_value; break;
          }

          if (result.HasValue()) {
            p(op);
            Propagate(result.Unwrap(), constant);
          }

          bool update_host_flags = add->update_host_flags;

          // Attempt to replace opcode with a MOV (removes dependencies on operand variables)
          if (result.HasValue()) {
            if (op_class == IROpcodeClass::ADD || op_class == IROpcodeClass::SUB) {
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

        break;
      }
      case IROpcodeClass::MUL: {
        const auto mul = (IRMultiply*)op.get();

        auto& lhs = GetKnownConstant(mul->lhs.Get());
        auto& rhs = GetKnownConstant(mul->rhs.Get());

        if (lhs.HasValue() && rhs.HasValue()) {
          if (mul->result_hi.HasValue()) {
            if (mul->lhs.Get().data_type == IRDataType::SInt32) {
              s64 result = (s64)(s32)lhs.Unwrap().value * (s64)(s32)rhs.Unwrap().value;
              IRConstant constant_lo = (u32)result;
              IRConstant constant_hi = (u32)(result >> 32);

              p(op);
              Propagate(mul->result_lo.Get(), constant_lo);
              Propagate(mul->result_hi.Unwrap(), constant_hi);
            } else {
              u64 result = (u64)lhs.Unwrap().value * (u64)rhs.Unwrap().value;
              IRConstant constant_lo = (u32)result;
              IRConstant constant_hi = (u32)(result >> 32);

              p(op);
              Propagate(mul->result_lo.Get(), constant_lo);
              Propagate(mul->result_hi.Unwrap(), constant_hi);
            }
          } else {
            IRConstant constant = lhs.Unwrap().value * rhs.Unwrap().value;

            p(op);
            Propagate(mul->result_lo.Get(), constant);
            op = MOV(mul->result_lo.Get(), constant, mul->update_host_flags);
          }
        }
        break;
      }
      case IROpcodeClass::MemoryRead: {
        const auto ldr = (IRMemoryRead*)op.get();

        auto& address = GetKnownConstant(ldr->address);

        if (address.HasValue()) {
          fmt::print("constant propagated to LDR address: {:08X}\n", address.Unwrap().value);
        }
        break;
      }
      case IROpcodeClass::MemoryWrite: {
        const auto str = (IRMemoryWrite*)op.get();

        auto& address = GetKnownConstant(str->address);

        if (address.HasValue()) {
          fmt::print("constant propagated to STR address: {:08X}\n", address.Unwrap().value);
        }
        break;
      }
    }
  }

}

} // namespace lunatic::frontend
} // namespace lunatic
