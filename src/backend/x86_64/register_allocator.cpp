/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <iterator>

#include "register_allocator.hpp"

using namespace lunatic::frontend;
using namespace Xbyak::util;

namespace lunatic {
namespace backend {

X64RegisterAllocator::X64RegisterAllocator(
  IREmitter const& emitter,
  Xbyak::CodeGenerator& code
) : emitter(emitter), code(code) {
  // Static allocation:
  //   - rax: host flags via lahf (overflow flag in al)
  //   - rbx: number of cycles left
  //   - rcx: pointer to guest state (lunatic::frontend::State)
  //   - rbp: pointer to stack frame / spill area.
  free_host_gprs = {
    edx,
    esi,
    edi,
    r8d,
    r9d,
    r10d,
    r11d,
    r12d,
    r13d,
    r14d,
    r15d
  };

  // XMM0 is statically allocated for GE flags.
  // TODO: use XMM6 to XMM31 when available.
  free_host_xmms = {
    xmm1,
    xmm2,
    xmm3,
    xmm4,
    xmm5,
  };

  auto number_of_vars = emitter.Vars().size();
  var_id_to_host_gpr.resize(number_of_vars);
  var_id_to_host_xmm.resize(number_of_vars);
  var_id_to_point_of_last_use.resize(number_of_vars);
  var_id_to_spill_slot.resize(number_of_vars);

  EvaluateVariableLifetimes();

  current_op_iter = emitter.Code().begin();
}

void X64RegisterAllocator::AdvanceLocation() {
  location++;
  current_op_iter++;

  // Release host regs that hold variables which now are dead.
  ReleaseDeadVariables();

  // Release host regs the previous opcode allocated temporarily.
  ReleaseTemporaryHostRegs();
}

auto X64RegisterAllocator::GetVariableGPR(IRVariable const& var) -> Xbyak::Reg32 {
  auto& maybe_gpr = var_id_to_host_gpr[var.id];
  
  if (maybe_gpr.HasValue()) {
    return maybe_gpr.Unwrap();
  }

  auto reg = FindFreeGPR();

  auto& maybe_xmm = var_id_to_host_xmm[var.id];
  auto& maybe_spill = var_id_to_spill_slot[var.id];

  if (maybe_spill.HasValue()) {
    auto slot = maybe_spill.Unwrap();
    code.mov(reg, dword[rbp + slot * sizeof(u32)]);
    free_spill_bitmap[slot] = false;
    maybe_spill = {};
  } else if (maybe_xmm.HasValue()) {
    code.movq(reg.cvt64(), maybe_xmm.Unwrap());
    maybe_xmm = {};
  }

  var_id_to_host_gpr[var.id] = reg;
  return reg;
}

auto X64RegisterAllocator::GetVariableXMM(lunatic::frontend::IRVariable const& var) -> Xbyak::Xmm {
  auto& maybe_xmm = var_id_to_host_xmm[var.id];
  
  if (maybe_xmm.HasValue()) {
    return maybe_xmm.Unwrap();
  }

  auto reg = FindFreeXMM();

  // If the variable is currently allocated to a GPR, move it from the GPR to the XMM register.
  auto& maybe_gpr = var_id_to_host_gpr[var.id];
  if (maybe_gpr.HasValue()) {
    code.movq(reg, maybe_gpr.Unwrap().cvt64());
    maybe_gpr = {};
  }

  var_id_to_host_xmm[var.id] = reg;
  return reg;
}

auto X64RegisterAllocator::GetScratchGPR() -> Xbyak::Reg32 {
  auto reg = FindFreeGPR();
  temp_host_gprs.push_back(reg);
  return reg;
}

auto X64RegisterAllocator::GetScratchXMM() -> Xbyak::Xmm {
  auto reg = FindFreeXMM();
  temp_host_xmms.push_back(reg);
  return reg;
}

void X64RegisterAllocator::ReleaseVarAndReuseGPR(
  IRVariable const& var_old,
  IRVariable const& var_new
) {
  if (var_id_to_host_gpr[var_new.id].HasValue()) {
    return;
  }

  auto point_of_last_use = var_id_to_point_of_last_use[var_old.id];

  if (point_of_last_use == location) {
    auto maybe_reg = var_id_to_host_gpr[var_old.id];

    if (maybe_reg.HasValue()) {
      var_id_to_host_gpr[var_new.id] = maybe_reg;
      var_id_to_host_gpr[var_old.id] = {};
    }
  }
}

bool X64RegisterAllocator::IsGPRFree(Xbyak::Reg64 reg) const {
  auto begin = free_host_gprs.begin();
  auto end = free_host_gprs.end();

  return std::find(begin, end, reg.cvt32()) != end;
}

void X64RegisterAllocator::EvaluateVariableLifetimes() {
  for (auto const& var : emitter.Vars()) {
    int point_of_last_use = -1;
    int location = 0;

    for (auto const& op : emitter.Code()) {
      if (op->Writes(*var) || op->Reads(*var)) {
        point_of_last_use = location;
      }

      location++;
    }

    if (point_of_last_use != -1) {
      var_id_to_point_of_last_use[var->id] = point_of_last_use;
    }
  }
}

void X64RegisterAllocator::ReleaseDeadVariables() {
  for (auto const& var : emitter.Vars()) {
    auto point_of_last_use = var_id_to_point_of_last_use[var->id];

    if (location > point_of_last_use) {
      auto maybe_reg = var_id_to_host_gpr[var->id];
      if (maybe_reg.HasValue()) {
        free_host_gprs.push_back(maybe_reg.Unwrap());
        var_id_to_host_gpr[var->id] = {};
      }
    }
  }
}

void X64RegisterAllocator::ReleaseTemporaryHostRegs() {
  for (auto reg : temp_host_gprs) {
    free_host_gprs.push_back(reg);
  }

  for (auto reg : temp_host_xmms) {
    free_host_xmms.push_back(reg);
  }

  temp_host_gprs.clear();
  temp_host_xmms.clear();
}

auto X64RegisterAllocator::FindFreeGPR() -> Xbyak::Reg32 {
  if (free_host_gprs.size() != 0) {
    auto reg = free_host_gprs.back();
    free_host_gprs.pop_back();
    return reg;
  }

  auto const& current_op = *current_op_iter;

  // Find a variable to be spilled and deallocate it.
  // TODO: think of a smart way to pick which variable/register to spill.
  for (auto const& var : emitter.Vars()) {
    if (var_id_to_host_gpr[var->id].HasValue()) {
      // Make sure the variable that we spill is not currently used.
      if (current_op->Reads(*var) || current_op->Writes(*var)) {
        continue;
      }

      auto reg = var_id_to_host_gpr[var->id].Unwrap();

      var_id_to_host_gpr[var->id] = {};

      // Spill the variable into one of the free slots.
      for (int slot = 0; slot < kSpillAreaSize; slot++) {
        if (!free_spill_bitmap[slot]) {
          code.mov(dword[rbp + slot * sizeof(u32)], reg);
          free_spill_bitmap[slot] = true;
          var_id_to_spill_slot[var->id] = slot;
          return reg;
        }
      }
    }
  }

  throw std::runtime_error("X64RegisterAllocator: out of GPRs and spill space.");
}

auto X64RegisterAllocator::FindFreeXMM() -> Xbyak::Xmm {
  if (free_host_xmms.size() != 0) {
    auto reg = free_host_xmms.back();
    free_host_xmms.pop_back();
    return reg;
  }

  throw std::runtime_error("X64RegisterAllocator: out of XMM registers.");
}

} // namespace lunatic::backend
} // namespace lunatic
