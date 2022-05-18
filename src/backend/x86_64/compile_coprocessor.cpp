/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileMRC(CompileContext const& context, IRReadCoprocessorRegister* op) {
  DESTRUCTURE_CONTEXT;

  code.push(rax);

  auto regs_saved = GetUsedHostRegsFromList(reg_alloc, {
    rcx, rdx, r8, r9, r10, r11,

    #ifdef ABI_SYSV
      rsi, rdi
    #endif
  });

  bool must_align_rsp = (regs_saved.size() % 2) == 1;

  Push(code, regs_saved);

#ifdef ABI_MSVC
  // the 'push' below changes the stack-alignment. 
  if (!must_align_rsp) {
    code.sub(rsp, sizeof(u64));
  }
  code.push(op->opcode2);
  code.sub(rsp, 0x20);
#else
  if (must_align_rsp) {
    code.sub(rsp, sizeof(u64));
  }
  code.mov(kRegArg4, op->opcode2);
#endif

  code.mov(kRegArg0, u64(coprocessors[op->coprocessor_id]));
  code.mov(kRegArg1.cvt32(), op->opcode1);
  code.mov(kRegArg2.cvt32(), op->cn);
  code.mov(kRegArg3.cvt32(), op->cm);

  code.mov(rax, u64(ReadCoprocessor));
  code.call(rax);

#ifdef ABI_MSVC
  if (must_align_rsp) {
    code.add(rsp, 0x28);
  } else {
    code.add(rsp, 0x30);
  }
#else
  if (must_align_rsp) {
    code.add(rsp, sizeof(u64));
  }
#endif

  Pop(code, regs_saved);

  code.mov(reg_alloc.GetVariableGPR(op->result.Get()), eax);
  code.pop(rax);
}

void X64Backend::CompileMCR(CompileContext const& context, IRWriteCoprocessorRegister* op) {
  DESTRUCTURE_CONTEXT;

  auto regs_saved = GetUsedHostRegsFromList(reg_alloc, {
    rax, rcx, rdx, r8, r9, r10, r11,

    #ifdef ABI_SYSV
    rsi, rdi
    #endif
  });

  bool must_align_rsp = (regs_saved.size() % 2) == 0;//1?

  Push(code, regs_saved);

  if (must_align_rsp) {
    code.sub(rsp, sizeof(u64));
  }

#ifdef ABI_MSVC
  if (op->value.IsConstant()) {
    code.push(op->value.GetConst().value);
  } else {
    code.push(reg_alloc.GetVariableGPR(op->value.GetVar()).cvt64());
  }
  code.push(op->opcode2);
  code.sub(rsp, 0x20);
#else
  if (op->value.IsConstant()) {
    code.mov(kRegArg5, op->value.GetConst().value);
  } else {
    code.mov(kRegArg5, reg_alloc.GetVariableGPR(op->value.GetVar()));
  }
  code.mov(kRegArg4, op->opcode2);
#endif

  code.mov(kRegArg0, u64(coprocessors[op->coprocessor_id]));
  code.mov(kRegArg1.cvt32(), op->opcode1);
  code.mov(kRegArg2.cvt32(), op->cn);
  code.mov(kRegArg3.cvt32(), op->cm);

  code.mov(rax, u64(WriteCoprocessor));
  code.call(rax);

#ifdef ABI_MSVC
  if (must_align_rsp) {
    code.add(rsp, 0x38);
  } else {
    code.add(rsp, 0x30);
  }
#else
  if (must_align_rsp) {
    code.add(rsp, sizeof(u64));
  }
#endif

  Pop(code, regs_saved);
}

} // namespace lunatic::backend
