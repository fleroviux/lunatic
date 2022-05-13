/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/translator/translator.hpp"

namespace lunatic {
namespace frontend {

auto Translator::Handle(ARMParallelSignedAdd16 const& opcode) -> Status {
  auto& result = emitter->CreateVar(IRDataType::UInt32, "result");
  auto& lhs = emitter->CreateVar(IRDataType::UInt32, "lhs");
  auto& rhs = emitter->CreateVar(IRDataType::UInt32, "rhs");
  
  emitter->LoadGPR(IRGuestReg{opcode.reg_lhs, mode}, lhs);
  emitter->LoadGPR(IRGuestReg{opcode.reg_rhs, mode}, rhs);
  emitter->SADD16(result, lhs, rhs);
  emitter->StoreGPR(IRGuestReg{opcode.reg_dst, mode}, result);

  EmitAdvancePC();
  return Status::Continue;
}

} // namespace lunatic::frontend
} // namespace lunatic
