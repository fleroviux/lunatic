/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "common.hpp"

namespace lunatic::frontend {

  struct ARMCoprocessorRegisterTransfer {
    Condition condition;

    bool load;
    GPR reg_dst;
    uint coprocessor_id;
    uint opcode1;
    uint cn;
    uint cm;
    uint opcode2;
  };

} // namespace lunatic::frontend
