/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/translator/translator.hpp"

namespace lunatic {
namespace frontend {

auto Translator::Handle(ARMParallelAddSub const& opcode) -> Status {
  using Op = ARMParallelAddSub::Opcode;

  auto& result = emitter->CreateVar(IRDataType::UInt32, "result");
  auto& lhs = emitter->CreateVar(IRDataType::UInt32, "lhs");
  auto& rhs = emitter->CreateVar(IRDataType::UInt32, "rhs");
  
  emitter->LoadGPR(IRGuestReg{opcode.reg_lhs, mode}, lhs);
  emitter->LoadGPR(IRGuestReg{opcode.reg_rhs, mode}, rhs);

  switch (opcode.opcode) {
    case Op::SADD16: emitter->PADDS16(result, lhs, rhs); break;
    case Op::QADD16: emitter->PQADDS16(result, lhs, rhs); break;
    case Op::UADD16: emitter->PADDU16(result, lhs, rhs); break;
  }

  emitter->StoreGPR(IRGuestReg{opcode.reg_dst, mode}, result);

  EmitUpdateGE();
  EmitAdvancePC();

  return Status::Continue;
}

} // namespace lunatic::frontend
} // namespace lunatic
