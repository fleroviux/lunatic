/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMBlockDataTransfer {
    Condition condition;

    bool pre_increment;
    bool add;
    bool user_mode;
    bool writeback;
    bool load;
    GPR  reg_base;
    u16  reg_list;
  };

} // namespace lunatic::frontend