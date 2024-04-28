/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "frontend/ir/emitter.hpp"

namespace lunatic {
namespace frontend {

struct IRPass {
  virtual ~IRPass() = default;

  virtual void Run(IREmitter& emitter) = 0;

protected:
  using InstructionList = IREmitter::InstructionList;

  bool Repoint(
    const IRVariable& var_old,
    const IRVariable& var_new,
    InstructionList::const_iterator begin,
    InstructionList::const_iterator end
  ) {
    if (var_old.data_type != var_new.data_type) {
      return false;
    }
    
    for (auto it = begin; it != end; ++it) {
      (*it)->Repoint(var_old, var_new); 
    }

    return true;
  }  
};

} // namespace lunatic::frontend
} // namespace lunatic
