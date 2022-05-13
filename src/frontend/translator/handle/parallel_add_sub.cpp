/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/translator/translator.hpp"

namespace lunatic {
namespace frontend {

auto Translator::Handle(ARMParallelSignedAdd16 const& opcode) -> Status {
  EmitAdvancePC();

  return Status::Continue;
}

} // namespace lunatic::frontend
} // namespace lunatic
