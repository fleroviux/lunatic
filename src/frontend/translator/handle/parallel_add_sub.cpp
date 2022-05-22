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
    case Op::SADD16:  emitter->PADDS16 (result, lhs, rhs); break;
    case Op::SSUB16:  emitter->PSUBS16 (result, lhs, rhs); break;
    case Op::SADD8:   emitter->PADDS8  (result, lhs, rhs); break;
    case Op::QADD16:  emitter->PQADDS16(result, lhs, rhs); break;
    case Op::QSUB16:  emitter->PQSUBS16(result, lhs, rhs); break;
    case Op::SHADD16: emitter->PHADDS16(result, lhs, rhs); break;
    case Op::SHSUB16: emitter->PHSUBS16(result, lhs, rhs); break;
    case Op::UADD16:  emitter->PADDU16 (result, lhs, rhs); break;
    case Op::USUB16:  emitter->PSUBU16 (result, lhs, rhs); break;
    case Op::UQADD16: emitter->PQADDU16(result, lhs, rhs); break;
    case Op::UQSUB16: emitter->PQSUBU16(result, lhs, rhs); break;
    case Op::UHADD16: emitter->PHADDU16(result, lhs, rhs); break;
    case Op::UHSUB16: emitter->PHSUBU16(result, lhs, rhs); break;
  }

  emitter->StoreGPR(IRGuestReg{opcode.reg_dst, mode}, result);

  EmitUpdateGE();
  EmitAdvancePC();

  return Status::Continue;
}

} // namespace lunatic::frontend
} // namespace lunatic
