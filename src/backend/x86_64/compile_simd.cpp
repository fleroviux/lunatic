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
  code.paddw(xmm2, xmm1);
  code.movd(result_reg, xmm2);

}

} // namespace lunatic::backend
