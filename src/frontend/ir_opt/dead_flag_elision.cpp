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
  /**
   * TODO:
   * a) implement the same logic for the Q-flag (update.q) and GE-flags (update.ge)
   * b) when we remove an update.nzcv opcode, turn ADDS into ADD (for example).
   *    - careful: ADC might still want to read the C-flag from that ADDS operation.
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

} // namespace lunatic::frontend
} // namespace lunatic
