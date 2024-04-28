/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lunatic/cpu.hpp>

namespace lunatic::frontend {

  enum class Condition {
    EQ = 0,
    NE = 1,
    CS = 2,
    CC = 3,
    MI = 4,
    PL = 5,
    VS = 6,
    VC = 7,
    HI = 8,
    LS = 9,
    GE = 10,
    LT = 11,
    GT = 12,
    LE = 13,
    AL = 14,
    NV = 15
  };

  enum class Shift {
    LSL = 0,
    LSR = 1,
    ASR = 2,
    ROR = 3
  };

  enum class Exception {
    Reset = 0x00,
    Undefined = 0x04,
    Supervisor = 0x08,
    PrefetchAbort = 0x0C,
    DataAbort = 0x10,
    IRQ = 0x18,
    FIQ = 0x1C
  };

} // namespace lunatic::frontend
