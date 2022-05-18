/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileFlush(CompileContext const& context, IRFlush* op) {
  DESTRUCTURE_CONTEXT;

  auto cpsr_reg = reg_alloc.GetVariableGPR(op->cpsr_in.Get());
  auto r15_in_reg = reg_alloc.GetVariableGPR(op->address_in.Get());
  auto r15_out_reg = reg_alloc.GetVariableGPR(op->address_out.Get());

  // Thanks to @wheremyfoodat (github.com/wheremyfoodat) for coming up with this.
  code.test(cpsr_reg, 1 << 5);
  code.sete(r15_out_reg.cvt8());
  code.movzx(r15_out_reg, r15_out_reg.cvt8());
  code.lea(r15_out_reg, dword[r15_in_reg + r15_out_reg * 4 + 4]);
}

void X64Backend::CompileFlushExchange(const CompileContext &context, IRFlushExchange *op) {
  DESTRUCTURE_CONTEXT;

  auto& address_in_var = op->address_in.Get();
  auto  address_in_reg = reg_alloc.GetVariableGPR(address_in_var);
  auto& cpsr_in_var = op->cpsr_in.Get();
  auto  cpsr_in_reg = reg_alloc.GetVariableGPR(cpsr_in_var);

  auto& address_out_var = op->address_out.Get();
  auto& cpsr_out_var = op->cpsr_out.Get();

  reg_alloc.ReleaseVarAndReuseGPR(address_in_var, address_out_var);
  reg_alloc.ReleaseVarAndReuseGPR(cpsr_in_var, cpsr_out_var);

  auto address_out_reg = reg_alloc.GetVariableGPR(address_out_var);
  auto cpsr_out_reg = reg_alloc.GetVariableGPR(cpsr_out_var);

  auto label_arm = Xbyak::Label{};
  auto label_done = Xbyak::Label{};

  if (address_out_reg != address_in_reg) {
    code.mov(address_out_reg, address_in_reg);
  }

  if (cpsr_out_reg != cpsr_in_reg) {
    code.mov(cpsr_out_reg, cpsr_in_reg);
  }

  // TODO: attempt to make this branchless.

  code.test(address_in_reg, 1);
  code.je(label_arm);

  // Thumb
  code.or_(cpsr_out_reg, 1 << 5);
  code.and_(address_out_reg, ~1);
  code.add(address_out_reg, sizeof(u16) * 2);
  code.jmp(label_done);

  // ARM
  code.L(label_arm);
  code.and_(cpsr_out_reg, ~(1 << 5));
  code.and_(address_out_reg, ~3);
  code.add(address_out_reg, sizeof(u32) * 2);

  code.L(label_done);
}

} // namespace lunatic::backend
