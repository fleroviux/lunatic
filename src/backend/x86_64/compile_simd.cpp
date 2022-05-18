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

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.paddw(result_reg, rhs_reg);

  // Calculate GE flags to XMM0
  auto scratch = reg_alloc.GetScratchXMM();
  code.movq(xmm0, lhs_reg);
  code.paddsw(xmm0, rhs_reg);
  code.pcmpeqw(scratch, scratch);
  code.pcmpgtw(xmm0, scratch);
}

void X64Backend::CompilePADDU16(CompileContext const& context, IRParallelAddU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.paddw(result_reg, rhs_reg);

  // Calculate GE flags to XMM0
  auto scratch = reg_alloc.GetScratchXMM();
  // scratch = 0x80008000
  code.pcmpeqw(scratch, scratch);
  code.psllw(scratch, 15);
  code.movq(xmm0, lhs_reg);
  code.paddw(xmm0, scratch);
  code.paddw(scratch, result_reg);
  code.pcmpgtw(xmm0, scratch);
}

void X64Backend::CompilePQADDS16(CompileContext const& context, IRParallelSaturateAddS16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.paddsw(result_reg, rhs_reg);
}

void X64Backend::CompilePQADDU16(CompileContext const& context, IRParallelSaturateAddU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.paddusw(result_reg, rhs_reg);
}

} // namespace lunatic::backend
