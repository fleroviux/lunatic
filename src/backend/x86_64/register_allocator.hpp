/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <bitset>
#include <xbyak/xbyak.h>
#include <vector>

#include "common/optional.hpp"
#include "frontend/ir/emitter.hpp"

namespace lunatic {
namespace backend {

struct X64RegisterAllocator {
  static constexpr int kSpillAreaSize = 32;

  X64RegisterAllocator(
    lunatic::frontend::IREmitter const& emitter,
    Xbyak::CodeGenerator& code
  );

  /**
   * Set the current location in the IR program.
   * 
   * @param  location The current location in the IR program.
   * @returns nothing
   */
  void SetCurrentLocation(int location);

  /**
   * Get the host register currently allocated to a variable.
   * 
   * @param  var  The variable
   * @returns the host register
   */
  auto GetVariableHostReg(
    lunatic::frontend::IRVariable const& var
  ) -> Xbyak::Reg32;

  /**
   * Get a temporary host register for use during the current opcode.
   * 
   * @returns the host register
   */
  auto GetTemporaryHostReg() -> Xbyak::Reg32;

private:
  /// Determine when each variable will be dead.
  void EvaluateVariableLifetimes();

  /// Release host registers allocated to variables that are dead.
  void ReleaseDeadVariables();

  /// Release host registers allocated for temporary storage.
  void ReleaseTemporaryHostRegs();

  /**
   * Find and allocate a host register that is currently unused.
   * If no register is free attempt to spill a variable to the stack to
   * free its register up.
   *
   * @returns the host register
   */
  auto FindFreeHostReg() -> Xbyak::Reg32;

  lunatic::frontend::IREmitter const& emitter;
  Xbyak::CodeGenerator& code;

  /// Host register that are free and can be allocated.
  std::vector<Xbyak::Reg32> free_host_regs;

  /// Map variable to its allocated host register (if any).
  std::vector<Optional<Xbyak::Reg32>> var_id_to_host_reg;
  
  /// Map variable to the last location where it's accessed.
  std::vector<int> var_id_to_point_of_last_use;

  /// The set of free/unused spill slots.
  std::bitset<kSpillAreaSize> free_spill_bitmap;

  /// Map variable to the slot it was spilled to (if it is spilled).  
  std::vector<Optional<int>> var_id_to_spill_slot;

  /// Array of currently allocated scratch registers.
  std::vector<Xbyak::Reg32> temp_host_regs;

  /// The current IR program location.
  int location = 0;
};

} // namespace lunatic::backend
} // namespace lunatic
