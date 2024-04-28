/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMMoveStatusRegister {
    Condition condition;
    bool immediate;
    bool spsr;
    int fsxc;
    GPR reg; // if immediate == false
    u32 imm; // if immediate == true
  };

  struct ARMMoveRegisterStatus {
    Condition condition;
    bool spsr;
    GPR reg;
  };

} // namespace lunatic::frontend
