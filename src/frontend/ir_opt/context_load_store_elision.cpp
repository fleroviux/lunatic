/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/ir_opt/context_load_store_elision.hpp"

namespace lunatic {
namespace frontend {

void IRContextLoadStoreElisionPass::Run(IREmitter& emitter) {
  // Forward pass: remove redundant GPR and CPSR reads
  RemoveLoads(emitter);

  // Backward pass: remove redundant GPR and CPSR stores
  RemoveStores(emitter);
}

void IRContextLoadStoreElisionPass::RemoveLoads(IREmitter& emitter) {
  auto& code = emitter.Code();
  auto it = code.begin();
  auto end = code.end();
  IRAnyRef current_gpr_value[512] {};
  IRAnyRef current_cpsr_value;

  auto Move = [&](const IRVariable& dst, IRAnyRef src) {
    code.insert(it, std::make_unique<IRMov>(dst, src, false));
  };

  while (it != end) {
    switch (it->get()->GetClass()) {
      case IROpcodeClass::StoreGPR: {
        auto op = lunatic_cast<IRStoreGPR>(it->get());
        auto gpr_id = op->reg.ID();

        current_gpr_value[gpr_id] = op->value;
        break;
      }
      case IROpcodeClass::LoadGPR: {
        auto  op = lunatic_cast<IRLoadGPR>(it->get());
        auto  gpr_id  = op->reg.ID();
        auto  var_src = current_gpr_value[gpr_id];
        auto& var_dst = op->result.Get();

        if (!var_src.IsNull()) {
          it = code.erase(it);

          // TODO: if var_src is constant attempt updating IRAnyRefs.
          if (var_src.IsConstant() || !Repoint(var_dst, var_src.GetVar(), it, end)) {
            Move(var_dst, var_src);
          }
          continue;
        } else {
          current_gpr_value[gpr_id] = var_dst;
        }
        break;
      }
      case IROpcodeClass::StoreCPSR: {
        current_cpsr_value = lunatic_cast<IRStoreCPSR>(it->get())->value;
        break;
      }
      case IROpcodeClass::LoadCPSR: {
        auto  op = lunatic_cast<IRLoadCPSR>(it->get());
        auto  var_src = current_cpsr_value;
        auto& var_dst = op->result.Get();

        if (!var_src.IsNull()) {
          it = code.erase(it);

          // TODO: if var_src is constant attempt updating IRAnyRefs.
          if (var_src.IsConstant() || !Repoint(var_dst, var_src.GetVar(), it, end)) {
            Move(var_dst, var_src);
          }
          continue;
        } else {
          current_cpsr_value = var_dst;
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

void IRContextLoadStoreElisionPass::RemoveStores(IREmitter& emitter) {
  auto& code = emitter.Code();
  auto it = code.rbegin();
  auto end = code.rend();
  bool gpr_already_stored[512] {false};
  bool cpsr_already_stored = false;

  while (it != end) {
    switch (it->get()->GetClass()) {
      case IROpcodeClass::StoreGPR: {
        auto op = lunatic_cast<IRStoreGPR>(it->get());
        auto gpr_id = op->reg.ID();

        if (gpr_already_stored[gpr_id]) {
          it = std::reverse_iterator{code.erase(std::next(it).base())};
          end = code.rend();
          continue;
        } else {
          gpr_already_stored[gpr_id] = true;
        }
        break;
      }
      case IROpcodeClass::StoreCPSR: {
        if (cpsr_already_stored) {
          it = std::reverse_iterator{code.erase(std::next(it).base())};
          end = code.rend();
          continue;
        } else {
          cpsr_already_stored = true;
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
