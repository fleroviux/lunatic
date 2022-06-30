/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileLoadGPR(CompileContext const& context, IRLoadGPR* op) {
  DESTRUCTURE_CONTEXT;

  auto address  = rcx + state.GetOffsetToGPR(op->reg.mode, op->reg.reg);
  auto host_reg = reg_alloc.GetVariableGPR(op->result.Get());

  code.mov(host_reg, dword[address]);
}

void X64Backend::CompileStoreGPR(CompileContext const& context, IRStoreGPR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToGPR(op->reg.mode, op->reg.reg);

  if (op->value.IsConstant()) {
    code.mov(dword[address], op->value.GetConst().value);
  } else {
    auto host_reg = reg_alloc.GetVariableGPR(op->value.GetVar());

    code.mov(dword[address], host_reg);
  }
}

void X64Backend::CompileLoadSPSR(CompileContext const& context, IRLoadSPSR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToSPSR(op->mode);
  auto host_reg = reg_alloc.GetVariableGPR(op->result.Get());

  code.mov(host_reg, dword[address]);
}

void X64Backend::CompileStoreSPSR(const CompileContext &context, IRStoreSPSR *op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToSPSR(op->mode);

  if (op->value.IsConstant()) {
    code.mov(dword[address], op->value.GetConst().value);
  } else {
    auto host_reg = reg_alloc.GetVariableGPR(op->value.GetVar());

    code.mov(dword[address], host_reg);
  }
}

void X64Backend::CompileLoadCPSR(CompileContext const& context, IRLoadCPSR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToCPSR();
  auto host_reg = reg_alloc.GetVariableGPR(op->result.Get());

  code.mov(host_reg, dword[address]);
}

void X64Backend::CompileStoreCPSR(CompileContext const& context, IRStoreCPSR* op) {
  DESTRUCTURE_CONTEXT;

  auto address = rcx + state.GetOffsetToCPSR();

  if (op->value.IsConstant()) {
    code.mov(dword[address], op->value.GetConst().value);
  } else {
    auto host_reg = reg_alloc.GetVariableGPR(op->value.GetVar());

    code.mov(dword[address], host_reg);
  }
}

} // namespace lunatic::backend
