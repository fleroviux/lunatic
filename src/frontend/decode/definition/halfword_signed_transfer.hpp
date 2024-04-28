/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMHalfwordSignedTransfer {
    Condition condition;

    bool pre_increment;
    bool add;
    bool immediate;
    bool writeback;
    // TODO: clean this up... there must be a nice way...
    bool load;
    int  opcode;

    GPR reg_dst;
    GPR reg_base;

    u32 offset_imm;
    GPR offset_reg;
  };

} // namespace lunatic::frontend
