/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileLSL(CompileContext const& context, IRLogicalShiftLeft* op) {
  DESTRUCTURE_CONTEXT;

  auto& amount = op->amount;
  auto& result_var = op->result.Get();
  auto& operand_var = op->operand.Get();
  auto operand_reg = reg_alloc.GetVariableGPR(operand_var);

  reg_alloc.ReleaseVarAndReuseGPR(operand_var, result_var);

  auto result_reg = reg_alloc.GetVariableGPR(result_var);

  if (result_reg != operand_reg) {
    code.mov(result_reg, operand_reg);
  }
  
  code.shl(result_reg.cvt64(), 32);

  if (amount.IsConstant()) {
    if (op->update_host_flags) {
      code.sahf();
    }
    code.shl(result_reg.cvt64(), u8(std::min(amount.GetConst().value, 33U)));
  } else {
    auto amount_reg = reg_alloc.GetVariableGPR(amount.GetVar());

    code.push(rcx);
    code.mov(cl, 33);
    code.cmp(amount_reg.cvt8(), u8(33));
    code.cmovl(ecx, amount_reg);

    if (op->update_host_flags) {
      code.sahf();
    }
    code.shl(result_reg.cvt64(), cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }

  code.shr(result_reg.cvt64(), 32);
}

void X64Backend::CompileLSR(CompileContext const& context, IRLogicalShiftRight* op) {
  DESTRUCTURE_CONTEXT;

  auto& amount = op->amount;
  auto& result_var = op->result.Get();
  auto& operand_var = op->operand.Get();
  auto operand_reg = reg_alloc.GetVariableGPR(operand_var);

  reg_alloc.ReleaseVarAndReuseGPR(operand_var, result_var);

  auto result_reg = reg_alloc.GetVariableGPR(result_var);
  
  if (result_reg != operand_reg) {
    code.mov(result_reg, operand_reg);
  }

  if (amount.IsConstant()) {
    auto amount_value = amount.GetConst().value;

    // LSR #0 equals to LSR #32
    if (amount_value == 0) {
      amount_value = 32;
    }

    if (op->update_host_flags) {
      code.sahf();
    }

    code.shr(result_reg.cvt64(), u8(std::min(amount_value, 33U)));
  } else {
    auto amount_reg = reg_alloc.GetVariableGPR(op->amount.GetVar());
    code.push(rcx);
    code.mov(cl, 33);
    code.cmp(amount_reg.cvt8(), u8(33));
    code.cmovl(ecx, amount_reg);
    if (op->update_host_flags) {
      code.sahf();
    }
    code.shr(result_reg.cvt64(), cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }
}

void X64Backend::CompileASR(CompileContext const& context, IRArithmeticShiftRight* op) {
  DESTRUCTURE_CONTEXT;

  auto& amount = op->amount;
  auto& result_var = op->result.Get();
  auto& operand_var = op->operand.Get();
  auto operand_reg = reg_alloc.GetVariableGPR(operand_var);

  reg_alloc.ReleaseVarAndReuseGPR(operand_var, result_var);

  auto result_reg = reg_alloc.GetVariableGPR(result_var);

  // Mirror sign-bit in the upper 32-bit of the full 64-bit register.
  code.movsxd(result_reg.cvt64(), operand_reg);

  // TODO: change shift amount saturation from 33 to 32 for ASR? 32 would also work I guess?

  if (amount.IsConstant()) {
    auto amount_value = amount.GetConst().value;

    // ASR #0 equals to ASR #32
    if (amount_value == 0) {
      amount_value = 32;
    }

    if (op->update_host_flags) {
      code.sahf();
    }

    code.sar(result_reg.cvt64(), u8(std::min(amount_value, 33U)));
  } else {
    auto amount_reg = reg_alloc.GetVariableGPR(op->amount.GetVar());
    code.push(rcx);
    code.mov(cl, 33);
    code.cmp(amount_reg.cvt8(), u8(33));
    code.cmovl(ecx, amount_reg);
    if (op->update_host_flags) {
      code.sahf();
    }
    code.sar(result_reg.cvt64(), cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }

  // Clear upper 32-bit of the result
  code.mov(result_reg, result_reg);
}

void X64Backend::CompileROR(CompileContext const& context, IRRotateRight* op) {
  DESTRUCTURE_CONTEXT;

  auto& amount = op->amount;
  auto& result_var = op->result.Get();
  auto& operand_var = op->operand.Get();
  auto operand_reg = reg_alloc.GetVariableGPR(operand_var);

  reg_alloc.ReleaseVarAndReuseGPR(operand_var, result_var);

  auto result_reg = reg_alloc.GetVariableGPR(result_var);
  auto label_done = Xbyak::Label{};

  if (result_reg != operand_reg) {
    code.mov(result_reg, operand_reg);
  }

  if (amount.IsConstant()) {
    auto amount_value = amount.GetConst().value;

    // ROR #0 equals to RRX #1
    if (amount_value == 0) {
      code.sahf();
      code.rcr(result_reg, 1);
    } else {
      if (op->update_host_flags) {
        code.sahf();
      }
      code.ror(result_reg, u8(amount_value));
    }
  } else {
    auto amount_reg = reg_alloc.GetVariableGPR(op->amount.GetVar());
    auto label_ok = Xbyak::Label{};

    // Handle (amount % 32) == 0 and amount == 0 cases.
    if (op->update_host_flags) {
      code.test(amount_reg.cvt8(), 31);
      code.jnz(label_ok);

      code.cmp(amount_reg.cvt8(), 0);
      code.jz(label_done);

      code.bt(result_reg, 31);
      code.lahf();
      code.jmp(label_done);
    }

    code.L(label_ok);
    code.push(rcx);
    code.mov(cl, amount_reg.cvt8());
    code.ror(result_reg, cl);
    code.pop(rcx);
  }

  if (op->update_host_flags) {
    code.lahf();
  }
  
  code.L(label_done);
}

} // namespace lunatic::backend
