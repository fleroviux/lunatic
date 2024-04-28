/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMSignedHalfwordMultiply {
    Condition condition;
    bool accumulate;
    bool x;
    bool y;
    GPR reg_dst;
    GPR reg_lhs;
    GPR reg_rhs;
    GPR reg_op3;
  };

  struct ARMSignedWordHalfwordMultiply {
    Condition condition;
    bool accumulate;
    bool y;
    GPR reg_dst;
    GPR reg_lhs;
    GPR reg_rhs;
    GPR reg_op3;
  };

  struct ARMSignedHalfwordMultiplyAccumulateLong {
    Condition condition;
    bool x;
    bool y;
    GPR reg_dst_hi;
    GPR reg_dst_lo;
    GPR reg_lhs;
    GPR reg_rhs;
  };

} // namespace lunatic::frontend
