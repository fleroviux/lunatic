/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileSADD16(CompileContext const& context, IRSignedAdd16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.Get());
  
  // TODO: save and restore XMM regs.

  code.movq(xmm1, lhs_reg.cvt64());
  code.movq(xmm2, rhs_reg.cvt64());
  code.movq(xmm3, xmm2);
  code.paddw(xmm3, xmm1);
  code.movd(result_reg, xmm3);

  code.movq(xmm0, xmm1);
  code.pxor(xmm0, xmm2);
  code.pxor(xmm2, xmm3);
  code.pandn(xmm0, xmm2);
  code.pxor(xmm0, xmm3);
  code.psraw(xmm0, 15);
  code.pcmpeqb(xmm1, xmm1); // FFFFFFFFFFFFFFFF
  code.pxor(xmm0, xmm1);

  // // Flag test
  // code.push(rcx);
  // code.mov(ecx, 0x80008000);
  // code.movd(xmm0, ecx);
  // code.pop(rcx);
}

} // namespace lunatic::backend
