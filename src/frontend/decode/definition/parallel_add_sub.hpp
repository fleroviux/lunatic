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

struct ARMParallelAddSub {
  Condition condition;

  enum class Opcode {
    SADD16  = 0b001000,
    SASX    = 0b001001,
    SSAX    = 0b001010,
    SSUB16  = 0b001011,
    SADD8   = 0b001100,
    SSUB8   = 0b001111,
    QADD16  = 0b010000,
    QASX    = 0b010001,
    QSAX    = 0b010010,
    QSUB16  = 0b010011,
    QADD8   = 0b010100,
    QSUB8   = 0b010111,
    SHADD16 = 0b011000,
    SHASX   = 0b011001,
    SHSAX   = 0b011010,
    SHSUB16 = 0b011011,
    SHADD8  = 0b011100,
    SHSUB8  = 0b011111,
    UADD16  = 0b101000,
    UASX    = 0b101001,
    USAX    = 0b101010,
    USUB16  = 0b101011,
    UADD8   = 0b101100,
    USUB8   = 0b101111,
    UQADD16 = 0b110000,
    UQASX   = 0b110001,
    UQSAX   = 0b110010,
    UQSUB16 = 0b110011,
    UQADD8  = 0b110100,
    UQSUB8  = 0b110111,
    UHADD16 = 0b111000,
    UHASX   = 0b111001,
    UHSAX   = 0b111010,
    UHSUB16 = 0b111011,
    UHADD8  = 0b111100,
    UHSUB8  = 0b111111
  } opcode;

  GPR reg_dst;
  GPR reg_lhs;
  GPR reg_rhs;
};

} // namespace lunatic::frontend
} // namespace lunatic
