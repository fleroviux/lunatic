/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMSaturatingAddSub {
    Condition condition;
    bool subtract;
    bool double_rhs;
    GPR reg_dst;
    GPR reg_lhs;
    GPR reg_rhs;
  };

} // namespace lunatic::frontend
