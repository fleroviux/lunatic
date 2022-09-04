/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <vector>

#include "frontend/ir_opt/pass.hpp"

namespace lunatic {
namespace frontend {

struct IRConstantPropagationPass final : IRPass {
  void Run(IREmitter& emitter) override;

private:
  void Propagate(IRVariable const& var, IRConstant const& constant);
  auto GetKnownConstant(IRVarRef const& var) -> Optional<IRConstant>&;

  void DoMOV(std::unique_ptr<IROpcode>& op);
  void DoLSL(std::unique_ptr<IROpcode>& op);
  void DoLSR(std::unique_ptr<IROpcode>& op);
  void DoASR(std::unique_ptr<IROpcode>& op);
  void DoROR(std::unique_ptr<IROpcode>& op);
  void DoMUL(std::unique_ptr<IROpcode>& op);

  template<typename OpcodeType>
  void DoBinaryOp(std::unique_ptr<IROpcode>& op);

  static auto MOV(IRVariable const& result, IRConstant const& constant, bool update_host_flags = false) {
    return std::make_unique<IRMov>(result, constant, update_host_flags);
  };

  static auto NOP() {
    return std::make_unique<IRNoOp>();
  };

  IREmitter* emitter;
  std::vector<Optional<IRConstant>> var_to_const{};
};

} // namespace lunatic::frontend
} // namespace lunatic
