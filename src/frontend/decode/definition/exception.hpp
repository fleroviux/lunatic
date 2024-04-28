/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMException {
    Condition condition;
    Exception exception;
    u32 svc_comment = 0;
  };

} // namespace lunatic::frontend
