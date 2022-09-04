/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "frontend/ir_opt/pass.hpp"

namespace lunatic {
namespace frontend {

struct IRDeadCodeElisionPass final : IRPass {
  void Run(IREmitter& emitter) override;

private:
  bool CheckMOV(IRMov* op);

  template<class OpcodeType>
  bool CheckShifterOp(OpcodeType* op);

  template<class OpcodeType>
  bool CheckBinaryOp(OpcodeType* op);

  bool CheckMUL(IRMultiply* op);

  bool IsValueUnused(IRVariable const& var);

  IREmitter* emitter;
  IREmitter::InstructionList::iterator it;
  IREmitter::InstructionList::iterator end;
};

} // namespace lunatic::frontend
} // namespace lunatic
