/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMSingleDataSwap {
    Condition condition;
    bool byte;
    GPR reg_src;
    GPR reg_dst;
    GPR reg_base;
  };

} // namespace lunatic::frontend
