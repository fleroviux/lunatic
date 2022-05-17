/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompilePADDS16(CompileContext const& context, IRParallelAddS16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.Get());
  
  // TODO: save and restore XMM regs.

  code.movq(xmm1, lhs_reg.cvt64());
  code.movq(xmm2, rhs_reg.cvt64());
  code.movq(xmm3, xmm1);
  code.paddw(xmm3, xmm2);
  code.movd(result_reg, xmm3);

  // Calculate GE flags to XMM0
  code.movq(xmm0, xmm1);
  code.paddsw(xmm0, xmm2);
  code.pcmpeqw(xmm1, xmm1);
  code.pcmpgtw(xmm0, xmm1);
}

void X64Backend::CompilePADDU16(CompileContext const& context, IRParallelAddU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.Get());

  // TODO: save and restore XMM regs.

  code.movq(xmm1, lhs_reg.cvt64());
  code.movq(xmm2, rhs_reg.cvt64());
  code.movq(xmm3, xmm1);
  code.paddw(xmm3, xmm2);
  code.movd(result_reg, xmm3);

  // Calculate GE flags to XMM0
  code.movq(xmm0, xmm1);
  code.pcmpeqw(xmm2, xmm2); // XMM2 = 1111111111111111 ...
  code.psllw(xmm2, 15);     // XMM2 = 1000000000000000 ...
  code.psubw(xmm0, xmm2);
  code.psubw(xmm3, xmm2);
  code.pcmpgtw(xmm0, xmm3);
}

} // namespace lunatic::backend
