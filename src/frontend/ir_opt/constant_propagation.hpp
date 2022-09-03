/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "frontend/ir_opt/pass.hpp"

namespace lunatic {
namespace frontend {

struct IRConstantPropagationPass final : IRPass {
  void Run(IREmitter& emitter) override;

private:
  //void Propagate(IREmitter& emitter, IRVariable const& var, IRConstant const& constant);
};

} // namespace lunatic::frontend
} // namespace lunatic
