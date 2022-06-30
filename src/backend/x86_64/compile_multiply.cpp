/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileMUL(CompileContext const& context, IRMultiply* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_lo_var = op->result_lo.Get();
  auto& lhs_var = op->lhs.Get();
  auto& rhs_var = op->rhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableGPR(op->lhs.Get());
  auto  rhs_reg = reg_alloc.GetVariableGPR(op->rhs.Get());

  if (op->result_hi.HasValue()) {
    auto result_lo_reg = reg_alloc.GetVariableGPR(result_lo_var);
    auto result_hi_reg = reg_alloc.GetVariableGPR(op->result_hi.Unwrap());
    auto rhs_ext_reg = reg_alloc.GetScratchGPR().cvt64();

    if (op->lhs.Get().data_type == IRDataType::SInt32) {
      code.movsxd(result_hi_reg.cvt64(), lhs_reg);
      code.movsxd(rhs_ext_reg, rhs_reg);
    } else {
      code.mov(result_hi_reg, lhs_reg);
      code.mov(rhs_ext_reg.cvt32(), rhs_reg);
    }

    code.imul(result_hi_reg.cvt64(), rhs_ext_reg);

    if (op->update_host_flags) {
      code.test(result_hi_reg.cvt64(), result_hi_reg.cvt64());
      code.lahf();
    }

    code.mov(result_lo_reg, result_hi_reg);
    code.shr(result_hi_reg.cvt64(), 32);
  } else {
    reg_alloc.ReleaseVarAndReuseGPR(lhs_var, result_lo_var);
    reg_alloc.ReleaseVarAndReuseGPR(rhs_var, result_lo_var);

    auto result_lo_reg = reg_alloc.GetVariableGPR(result_lo_var);

    if (result_lo_reg == lhs_reg) {
      code.imul(lhs_reg, rhs_reg);
    } else if (result_lo_reg == rhs_reg) {
      code.imul(rhs_reg, lhs_reg);
    } else {
      code.mov(result_lo_reg, lhs_reg);
      code.imul(result_lo_reg, rhs_reg);
    }

    if (op->update_host_flags) {
      code.test(result_lo_reg, result_lo_reg);
      code.lahf();
    }
  }
}

void X64Backend::CompileADD64(CompileContext const& context, IRAdd64* op) {
  DESTRUCTURE_CONTEXT;

  auto& lhs_hi_var = op->lhs_hi.Get();
  auto& lhs_lo_var = op->lhs_lo.Get();
  auto  lhs_hi_reg = reg_alloc.GetVariableGPR(lhs_hi_var);
  auto  lhs_lo_reg = reg_alloc.GetVariableGPR(lhs_lo_var);

  auto& rhs_hi_var = op->rhs_hi.Get();
  auto& rhs_lo_var = op->rhs_lo.Get();
  auto  rhs_hi_reg = reg_alloc.GetVariableGPR(rhs_hi_var);
  auto  rhs_lo_reg = reg_alloc.GetVariableGPR(rhs_lo_var);

  auto& result_hi_var = op->result_hi.Get();
  auto& result_lo_var = op->result_lo.Get();

  if (op->update_host_flags) {
    reg_alloc.ReleaseVarAndReuseGPR(lhs_hi_var, result_hi_var);
    reg_alloc.ReleaseVarAndReuseGPR(rhs_hi_var, result_lo_var);

    auto result_hi_reg = reg_alloc.GetVariableGPR(result_hi_var);
    auto result_lo_reg = reg_alloc.GetVariableGPR(result_lo_var);

    // Pack (lhs_hi, lhs_lo) into result_hi
    if (result_hi_reg != lhs_hi_reg) {
      code.mov(result_hi_reg, lhs_hi_reg);
    }
    code.shl(result_hi_reg.cvt64(), 32);
    code.or_(result_hi_reg.cvt64(), lhs_lo_reg);

    // Pack (rhs_hi, rhs_lo) into result_lo
    if (result_lo_reg != rhs_hi_reg) {
      code.mov(result_lo_reg, rhs_hi_reg);
    }
    code.shl(result_lo_reg.cvt64(), 32);
    code.or_(result_lo_reg.cvt64(), rhs_lo_reg);

    code.add(result_hi_reg.cvt64(), result_lo_reg.cvt64());
    code.lahf();
  
    code.mov(result_lo_reg, result_hi_reg);
    code.shr(result_hi_reg.cvt64(), 32);
  } else {
    reg_alloc.ReleaseVarAndReuseGPR(lhs_lo_var, result_lo_var);
    reg_alloc.ReleaseVarAndReuseGPR(lhs_hi_var, result_hi_var);

    auto result_hi_reg = reg_alloc.GetVariableGPR(result_hi_var);
    auto result_lo_reg = reg_alloc.GetVariableGPR(result_lo_var);

    if (result_lo_reg != lhs_lo_reg) {
      code.mov(result_lo_reg, lhs_lo_reg);
    }
    if (result_hi_reg != lhs_hi_reg) {
      code.mov(result_hi_reg, lhs_hi_reg);
    }

    code.add(result_lo_reg, rhs_lo_reg);
    code.adc(result_hi_reg, rhs_hi_reg);
  }
}

} // namespace lunatic::backend
