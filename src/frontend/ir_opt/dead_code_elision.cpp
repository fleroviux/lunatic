/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/ir_opt/dead_code_elision.hpp"

namespace lunatic {
namespace frontend {

void IRDeadCodeElisionPass::Run(IREmitter& emitter) {
	auto& code = emitter.Code();
	auto it = code.begin();
  auto end = code.end();

  const auto WillVarBeRead = [&](IRVariable const& var) {
    // TODO: do not entire all opcodes, this is wasteful.
    for (auto& op : code) {
      if (op->Reads(var))
        return true;
    }
    return false;
  };

  while (it != end) {
    const auto op_class = it->get()->GetClass();

    // Handle opcodes that become redundant when their result variable is unread
    switch (op_class) {
      case IROpcodeClass::MOV: {
        auto op = lunatic_cast<IRMov>(it->get());

        if (!WillVarBeRead(op->result.Get()) && !op->update_host_flags) {
          it = code.erase(it);
          fmt::print("removed: {}\n", op->ToString());
          continue;
        }
        break;
      }
      case IROpcodeClass::ADD:
      case IROpcodeClass::SUB:
      case IROpcodeClass::AND:
      case IROpcodeClass::BIC:
      case IROpcodeClass::EOR:
      case IROpcodeClass::ORR: {
        auto op = (IRAdd*)it->get();

        if ((!op->result.HasValue() ||!WillVarBeRead(op->result.Unwrap())) && !op->update_host_flags) {
          fmt::print("removed: {}\n", op->ToString());
          it = code.erase(it);
          continue;
        }
        break;
      }
    }

    switch (op_class) {
      case IROpcodeClass::ADD: {
        auto op = lunatic_cast<IRAdd>(it->get());

        // ADD #0 is a no-operation
        if (op->result.HasValue() && op->rhs.IsConstant() && op->rhs.GetConst().value == 0 && !op->update_host_flags) {
          if (Repoint(op->result.Unwrap(), op->lhs.Get(), it, end)) {
            it = code.erase(it);
            continue;
          }
        }
        break;
      }
      // LSL(S) #0 is a no-operation
      case IROpcodeClass::LSL: {
        auto op = lunatic_cast<IRLogicalShiftLeft>(it->get());
        
        if (op->amount.IsConstant() && op->amount.GetConst().value == 0) {
          if (Repoint(op->result.Get(), op->operand.Get(), it, end)) {
            it = code.erase(it);
            continue;
          }
        }
        break;
      }
      case IROpcodeClass::MOV: {
        auto op = lunatic_cast<IRMov>(it->get());

        // MOV var_a, var_b: var_a is a redundant variable.
        if (op->source.IsVariable() && !op->update_host_flags) {
          if (Repoint(op->result.Get(), op->source.GetVar(), it, end)) {
            it = code.erase(it);
            continue;
          }
        }
        break;
      }
      default: {
      	break;
      }
    }

    ++it;
  }
}

} // namespace lunatic::frontend
} // namespace lunatic