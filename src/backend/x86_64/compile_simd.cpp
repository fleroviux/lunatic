/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompilePADDS8(CompileContext const& context, IRParallelAddS8* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.paddb(result_reg, rhs_reg);

  // Calculate GE flags to XMM0
  auto scratch = reg_alloc.GetScratchXMM();
  code.movq(xmm0, lhs_reg);
  code.paddsb(xmm0, rhs_reg);
  code.pcmpeqb(scratch, scratch);
  code.pcmpgtb(xmm0, scratch);
}

void X64Backend::CompilePADDU8(CompileContext const& context, IRParallelAddU8* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.paddb(result_reg, rhs_reg);

  // Calculate GE flags to XMM0
  auto scratch = reg_alloc.GetScratchXMM();
  auto scratch_gpr = reg_alloc.GetScratchGPR();
  // scratch = 0x80808080
  // code.pcmpeqb(scratch, scratch);
  // code.psllb(scratch, 7);
  code.mov(scratch_gpr, 0x80808080);
  code.movd(scratch, scratch_gpr);
  code.movq(xmm0, lhs_reg);
  code.paddb(xmm0, scratch);
  code.paddb(scratch, result_reg);
  code.pcmpgtb(xmm0, scratch);
}

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

void X64Backend::CompilePSUBS16(CompileContext const& context, IRParallelSubS16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.psubw(result_reg, rhs_reg);

  // Calculate GE flags to XMM0
  auto scratch = reg_alloc.GetScratchXMM();
  code.movq(xmm0, lhs_reg);
  code.psubsw(xmm0, rhs_reg);
  code.pcmpeqw(scratch, scratch);
  code.pcmpgtw(xmm0, scratch);
}

void X64Backend::CompilePSUBU16(CompileContext const& context, IRParallelSubU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.psubw(result_reg, rhs_reg);

  // Calculate GE flags to XMM0
  // TODO: make this more efficient.
  auto scratch0 = reg_alloc.GetScratchXMM();
  auto scratch1 = reg_alloc.GetScratchXMM();
  // scratch0 = 0x80008000
  // scratch1 = 0x00010001
  code.pcmpeqw(scratch0, scratch0);
  code.psllw(scratch0, 15);
  code.movq(scratch1, scratch0);
  code.psrlw(scratch1, 15);
  code.movq(xmm0, lhs_reg);
  code.paddw(xmm0, scratch0);
  code.paddw(xmm0, scratch1);
  code.paddw(scratch0, rhs_reg);
  code.pcmpgtw(xmm0, scratch0);
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

void X64Backend::CompilePQSUBS16(CompileContext const& context, IRParallelSaturateSubS16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.psubsw(result_reg, rhs_reg);
}

void X64Backend::CompilePQSUBU16(CompileContext const& context, IRParallelSaturateSubU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());

  code.movq(result_reg, lhs_reg);
  code.psubusw(result_reg, rhs_reg);
}

void X64Backend::CompilePHADDS16(CompileContext const& context, IRParallelHalvingAddS16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());
  auto scratch = reg_alloc.GetScratchXMM();

  //  a + b = (a ^ b) + ((a & b) << 1)
  // (a + b) >>> 1 = ((a ^ b) >>> 1) + (a & b)
  code.movq(result_reg, lhs_reg);
  code.pxor(result_reg, rhs_reg);
  code.psraw(result_reg, 1);
  code.movq(scratch, lhs_reg);
  code.pand(scratch, rhs_reg);
  code.paddw(result_reg, scratch);
}
 
void X64Backend::CompilePHADDU16(CompileContext const& context, IRParallelHalvingAddU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());
  auto scratch = reg_alloc.GetScratchXMM();

  //  a + b = (a ^ b) + ((a & b) << 1)
  // (a + b) >> 1 = ((a ^ b) >> 1) + (a & b)
  code.movq(result_reg, lhs_reg);
  code.pxor(result_reg, rhs_reg);
  code.psrlw(result_reg, 1);
  code.movq(scratch, lhs_reg);
  code.pand(scratch, rhs_reg);
  code.paddw(result_reg, scratch);
}

void X64Backend::CompilePHSUBS16(CompileContext const& context, IRParallelHalvingSubS16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());
  auto scratch = reg_alloc.GetScratchXMM();

  // calculate MSB (bit 15) of result
  code.movq(result_reg, lhs_reg);
  code.psubsw(result_reg, rhs_reg);
  code.psrlw(result_reg, 15);
  code.psllw(result_reg, 15);

  // calculate lower 15 bits of the result and OR the MSB.
  code.movq(scratch, lhs_reg);
  code.psubw(scratch, rhs_reg);
  code.psrlw(scratch, 1);
  code.por(result_reg, scratch);
}
 
void X64Backend::CompilePHSUBU16(CompileContext const& context, IRParallelHalvingSubU16* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableXMM(op->result.Get());
  auto lhs_reg = reg_alloc.GetVariableXMM(op->lhs.Get());
  auto rhs_reg = reg_alloc.GetVariableXMM(op->rhs.Get());
  auto scratch = reg_alloc.GetScratchXMM();

  // calculate MSB (bit 15) of result
  code.pcmpeqb(scratch, scratch);
  code.psllw(scratch, 15); // = 0x80008000
  code.movq(result_reg, lhs_reg);
  code.pxor(result_reg, scratch);
  code.pxor(scratch, rhs_reg);
  code.psubsw(result_reg, scratch);
  code.psrlw(result_reg, 15);
  code.psllw(result_reg, 15);

  // calculate lower 15 bits of the result and OR the MSB.
  code.movq(scratch, lhs_reg);
  code.psubw(scratch, rhs_reg);
  code.psrlw(scratch, 1);
  code.por(result_reg, scratch);
}

} // namespace lunatic::backend
