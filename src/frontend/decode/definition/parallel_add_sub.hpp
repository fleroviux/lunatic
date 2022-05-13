/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic {
namespace frontend {

struct ARMParallelSignedAdd16 {
  Condition condition;
  GPR reg_dst;
  GPR reg_lhs;
  GPR reg_rhs;
};

} // namespace lunatic::frontend
} // namespace lunatic
