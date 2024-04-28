/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMDataProcessing {
    Condition condition;

    enum class Opcode {
      AND = 0,
      EOR = 1,
      SUB = 2,
      RSB = 3,
      ADD = 4,
      ADC = 5,
      SBC = 6,
      RSC = 7,
      TST = 8,
      TEQ = 9,
      CMP = 10,
      CMN = 11,
      ORR = 12,
      MOV = 13,
      BIC = 14,
      MVN = 15
    } opcode;

    bool immediate;
    bool set_flags;

    GPR reg_dst;
    GPR reg_op1;

    /// Valid if immediate = false
    struct {
      GPR reg;
      struct {
        Shift type;
        bool immediate;
        GPR  amount_reg;
        uint amount_imm;
      } shift;
    } op2_reg;

    /// Valid if immediate = true
    struct {
      u32  value;
      uint shift;
    } op2_imm;

    bool thumb_load_address = false;
  };

} // namespace lunatic::frontend
