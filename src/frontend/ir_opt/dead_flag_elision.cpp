/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "frontend/ir_opt/dead_flag_elision.hpp"

namespace lunatic {
namespace frontend {

void IRDeadFlagElisionPass::Run(IREmitter& emitter) {
  RemoveRedundantUpdateFlagsOpcodes(emitter);
  DisableRedundantFlagCalculations(emitter);
}

void IRDeadFlagElisionPass::RemoveRedundantUpdateFlagsOpcodes(IREmitter& emitter) {
  /**
   * TODO: implement the same logic for the Q-flag (update.q)
   */

  bool unused_n = false;
  bool unused_z = false;
  bool unused_c = false;
  bool unused_v = false;

  auto& code = emitter.Code();
  auto it = code.rbegin();
  auto end = code.rend();

  Optional<IRVariable const&> current_cpsr_in{};

  while (it != end) {
    switch (it->get()->GetClass()) {
      case IROpcodeClass::UpdateFlags: {
        auto op = lunatic_cast<IRUpdateFlags>(it->get());

        if (current_cpsr_in.HasValue() && op->Writes(current_cpsr_in.Unwrap())) {
          if (unused_n) op->flag_n = false;
          if (unused_z) op->flag_z = false;
          if (unused_c) op->flag_c = false;
          if (unused_v) op->flag_v = false;

          // Elide update.nzcv opcodes that now don't update any flag.
          if (!op->flag_n && !op->flag_z && !op->flag_c && !op->flag_v) {
            // TODO: do not use begin()?
            if (Repoint(op->result.Get(), op->input.Get(), code.begin(), code.end())) {
              current_cpsr_in = op->input.Get();
              it = std::reverse_iterator{code.erase(std::next(it).base())};
              end = code.rend();
              continue;
            }
          }
        }

        if (op->flag_n) unused_n = true;
        if (op->flag_z) unused_z = true;
        if (op->flag_c) unused_c = true;
        if (op->flag_v) unused_v = true;

        current_cpsr_in = op->input.Get();

        break;
      }
      default: {
        if (current_cpsr_in.HasValue() && it->get()->Reads(current_cpsr_in.Unwrap())) {
          /* We are reading the CPSR value between two NZCV updates.
           * This means that the first NZCV update must update all flags,
           * otherwise this opcode will read the wrong CPSR value.
           */
          unused_n = false;
          unused_z = false;
          unused_c = false;
          unused_v = false;
          current_cpsr_in = {};
        }
        break;
      }
    }

    ++it;
  }
}

void IRDeadFlagElisionPass::DisableRedundantFlagCalculations(IREmitter& emitter) {
  auto& code = emitter.Code();
  auto it = code.rbegin();
  auto end = code.rend();

  bool used_n = false;
  bool used_z = false;
  bool used_c = false;
  bool used_v = false;

  while (it != end) {
    auto op_class = it->get()->GetClass();

    switch (op_class) {
      case IROpcodeClass::UpdateFlags: {
        auto op = lunatic_cast<IRUpdateFlags>(it->get());

        if (op->flag_n) used_n = true;
        if (op->flag_z) used_z = true;
        if (op->flag_c) used_c = true;
        if (op->flag_v) used_v = true;
        break;
      }
      case IROpcodeClass::UpdateSticky: {
        used_v = true; // Q-flag is kept in the host V-flag
        break;
      }
      case IROpcodeClass::ClearCarry:
      case IROpcodeClass::SetCarry: {
        if (!used_c) {
          *it = std::make_unique<IRNoOp>();
        }
        used_c = false;
        break;
      }
      case IROpcodeClass::LSL:
      case IROpcodeClass::LSR:
      case IROpcodeClass::ASR:
      case IROpcodeClass::ROR: {
        auto op = (IRLogicalShiftLeft*)it->get();

        if (!used_c) {
          op->update_host_flags = false;
        } else if (op->update_host_flags && op->amount.IsConstant() && op_class != IROpcodeClass::LSL) {
          used_c = false;
        }

        // Special case: RRX #1 (encoded as ROR #0) reads the carry flag.
        if (op_class == IROpcodeClass::ROR && op->amount.IsConstant() && op->amount.GetConst().value == 0) {
          used_c = true;
        }
        break;
      }
      case IROpcodeClass::AND:
      case IROpcodeClass::BIC:
      case IROpcodeClass::EOR:
      case IROpcodeClass::ORR: {
        auto op = (IRBitwiseAND*)it->get();

        if (!used_n && !used_z) {
          op->update_host_flags = false;
        } else if (op->update_host_flags) {
          used_n = false;
          used_z = false;
        }
        break;
      }
      case IROpcodeClass::ADD:
      case IROpcodeClass::SUB:
      case IROpcodeClass::RSB: {
        auto op = (IRAdd*)it->get();

        if (!used_n && !used_z && !used_c && !used_v) {
          op->update_host_flags = false;
        } else if (op->update_host_flags) {
          used_n = false;
          used_z = false;
          used_c = false;
          used_v = false;
        }
        break;
      }
      case IROpcodeClass::ADC:
      case IROpcodeClass::SBC:
      case IROpcodeClass::RSC: {
        auto op = (IRAdc*)it->get();

        if (!used_n && !used_z && !used_c && !used_v) {
          op->update_host_flags = false;
        } else if (op->update_host_flags) {
          used_n = false;
          used_z = false;
          used_v = false;
        }

        used_c = true;
        break;
      }
      case IROpcodeClass::MOV:
      case IROpcodeClass::MVN: {
        auto op = (IRMov*)it->get();

        if (!used_n && !used_z) {
          op->update_host_flags = false;
        } else if (op->update_host_flags) {
          used_n = false;
          used_z = false;
        }
        break;
      }
      case IROpcodeClass::MUL: {
        auto op = lunatic_cast<IRMultiply>(it->get());

        if (!used_n && !used_z) {
          op->update_host_flags = false;
        } else if (op->update_host_flags) {
          used_n = false;
          used_z = false;
        }
        break;
      }
      case IROpcodeClass::ADD64: {
        auto op = lunatic_cast<IRAdd64>(it->get());

        if (!used_n && !used_z) {
          op->update_host_flags = false;
        } else if (op->update_host_flags) {
          used_n = false;
          used_z = false;
        }
        break;
      }
      case IROpcodeClass::QADD:
      case IROpcodeClass::QSUB: {
        used_v = true;
        break;
      }
    }

    ++it;
  }
}

} // namespace lunatic::frontend
} // namespace lunatic
