/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMSingleDataTransfer {
    Condition condition;

    bool immediate;
    bool pre_increment;
    bool add;
    bool byte;
    bool writeback;
    bool load;

    GPR reg_dst;
    GPR reg_base;

    u32 offset_imm;

    struct {
      GPR reg;
      Shift shift;
      u32 amount;
    } offset_reg;
  };

} // namespace lunatic::frontend
