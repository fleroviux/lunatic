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
  this->emitter = &emitter;
  it = code.begin();
  end = code.end();

  while (it != end) {
    bool dead = false;

    switch (it->get()->GetClass()) {
      case IROpcodeClass::MOV: dead = CheckMOV(lunatic_cast<IRMov>(it->get())); break;
      case IROpcodeClass::LSL: dead = CheckShifterOp(lunatic_cast<IRLogicalShiftLeft>(it->get())); break;
      case IROpcodeClass::LSR: dead = CheckShifterOp(lunatic_cast<IRLogicalShiftRight>(it->get())); break;
      case IROpcodeClass::ASR: dead = CheckShifterOp(lunatic_cast<IRArithmeticShiftRight>(it->get())); break;
      case IROpcodeClass::ROR: dead = CheckShifterOp(lunatic_cast<IRRotateRight>(it->get())); break;
      case IROpcodeClass::ADD: dead = CheckBinaryOp(lunatic_cast<IRAdd>(it->get())); break;
      case IROpcodeClass::SUB: dead = CheckBinaryOp(lunatic_cast<IRSub>(it->get())); break;
      case IROpcodeClass::AND: dead = CheckBinaryOp(lunatic_cast<IRBitwiseAND>(it->get())); break;
      case IROpcodeClass::BIC: dead = CheckBinaryOp(lunatic_cast<IRBitwiseBIC>(it->get())); break;
      case IROpcodeClass::EOR: dead = CheckBinaryOp(lunatic_cast<IRBitwiseEOR>(it->get())); break;
      case IROpcodeClass::ORR: dead = CheckBinaryOp(lunatic_cast<IRBitwiseORR>(it->get())); break;
      case IROpcodeClass::MUL: dead = CheckMUL(lunatic_cast<IRMultiply>(it->get())); break;
    }

    if (dead) {
      it = code.erase(it);
    } else {
      ++it;
    }
  }
}

bool IRDeadCodeElisionPass::CheckMOV(IRMov* op) {
  if (IsValueUnused(op->result.Get()) && !op->update_host_flags) {
    return true;
  }

  // MOV var_a, var_b: var_a is a redundant variable.
  if (op->source.IsVariable() &&
      !op->update_host_flags &&
    Repoint(op->result.Get(), op->source.GetVar(), it, end)
  ) {
    return true;
  }

  return false;
}

template<class OpcodeType>
bool IRDeadCodeElisionPass::CheckShifterOp(OpcodeType* op) {
  if (IsValueUnused(op->result.Get()) && !op->update_host_flags) {
    return true;
  }

  // LSL #0 is a no-operation
  if constexpr(OpcodeType::klass == IROpcodeClass::LSL) {
    if (op->amount.IsConstant() &&
        op->amount.GetConst().value == 0 &&
        Repoint(op->result.Get(), op->operand.Get(), it, end)
    ) {
      return true;
    }
  }

  return false;
}

template<class OpcodeType>
bool IRDeadCodeElisionPass::CheckBinaryOp(OpcodeType* op) {
  if ((!op->result.HasValue() ||IsValueUnused(op->result.Unwrap())) && !op->update_host_flags) {
    return true;
  }

  // ADD #0 is a no-operation
  if constexpr(OpcodeType::klass == IROpcodeClass::ADD) {
    if (op->result.HasValue() &&
        op->rhs.IsConstant() &&
        op->rhs.GetConst().value == 0 &&
        !op->update_host_flags &&
        Repoint(op->result.Unwrap(), op->lhs.Get(), it, end)
    ) {
      return true;
    }
  }

  return false;
}

bool IRDeadCodeElisionPass::CheckMUL(IRMultiply* op) {
  if (IsValueUnused(op->result_lo.Get()) &&
      (!op->result_hi.HasValue() || IsValueUnused(op->result_hi.Unwrap())) &&
      !op->update_host_flags
  ) {
    return true;
  }

  return false;
}

bool IRDeadCodeElisionPass::IsValueUnused(IRVariable const& var) {
  auto local_it = it;

  ++local_it;

  while (local_it != end) {
    if (local_it->get()->Reads(var))
      return false;
    ++local_it;
  }

  return true;
}

} // namespace lunatic::frontend
} // namespace lunatic