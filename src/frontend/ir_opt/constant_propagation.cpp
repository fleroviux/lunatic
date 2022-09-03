/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <vector>

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

  for (auto const& op : emitter.Code()) {
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

        if (result.HasValue() && lhs.HasValue() && rhs.IsConstant()) {
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

          p(op);
          Propagate(result.Unwrap(), constant);
        }

        break;
      }
    }
  }

}

} // namespace lunatic::frontend
} // namespace lunatic
