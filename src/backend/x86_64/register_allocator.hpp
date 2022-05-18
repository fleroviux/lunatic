/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <bitset>
#include <vector>

#ifdef LUNATIC_INCLUDE_XBYAK_FROM_DIRECTORY
  #include <xbyak/xbyak.h>
#else
  #include <xbyak.h>
#endif

#include "common/optional.hpp"
#include "frontend/ir/emitter.hpp"

namespace lunatic {
namespace backend {

struct X64RegisterAllocator {
  using IREmitter = lunatic::frontend::IREmitter;

  static constexpr int kSpillAreaSize = 32;

  X64RegisterAllocator(
    IREmitter const& emitter,
    Xbyak::CodeGenerator& code
  );

  /**
   * Advance to the next IR opcode in the IR program.
   */
  void AdvanceLocation();

  /**
   * Get the host GPR currently allocated to a variable.
   * Allocate a host GPR is the variable has not been allocated yet.
   * 
   * @param  var  The variable
   * @returns the host GPR
   */
  auto GetVariableGPR(
    lunatic::frontend::IRVariable const& var
  ) -> Xbyak::Reg32;

  /**
   * Get a scratch host GPR for use during the current opcode.
   * The host GPR will be automatically released after the current opcode.
   * 
   * @returns the host GPR
   */
  auto GetScratchGPR() -> Xbyak::Reg32;

  /**
   * Get a scratch host XMM register for use during the current opcode.
   * The host XMM register will be automatically released after the current opcode.
   * 
   * @returns the host XMM register
   */
  auto GetScratchXMM() -> Xbyak::Xmm;

  /**
   * If var_old will be released after the current opcode,
   * then it will be released early and the host GPR
   * allocated to it will be moved to var_new.
   * The caller is responsible to not read var_old after writing var_new.
   * 
   * @param  var_old  the variable to release
   * @param  var_new  the variable to receive the released host GPR
   */
  void ReleaseVarAndReuseGPR(
    lunatic::frontend::IRVariable const& var_old,
    lunatic::frontend::IRVariable const& var_new
  );

  bool IsGPRFree(Xbyak::Reg64 reg) const;

private:
  /// Determine when each variable will be dead.
  void EvaluateVariableLifetimes();

  /// Release host registers allocated to variables that are dead.
  void ReleaseDeadVariables();

  /// Release host registers allocated for temporary storage.
  void ReleaseTemporaryHostRegs();

  /**
   * Find and allocate a host GPR that is currently unused.
   * If no register is free attempt to spill a variable to the stack to
   * free its register up.
   *
   * @returns the host register
   */
  auto FindFreeGPR() -> Xbyak::Reg32;

  /**
   * Find and allocate a host XMM register that is currently unused.
   *
   * @returns the host XMM register
   */
  auto FindFreeXMM() -> Xbyak::Xmm;

  IREmitter const& emitter;
  Xbyak::CodeGenerator& code;

  /// Host GPRs that are free and can be allocated.
  std::vector<Xbyak::Reg32> free_host_gprs;

  // Host XMM regs that are free and can be allocated.
  std::vector<Xbyak::Xmm> free_host_xmms;

  /// Map variable to its allocated host GPR (if any).
  std::vector<Optional<Xbyak::Reg32>> var_id_to_host_gpr;
  
  /// Map variable to the last location where it's accessed.
  std::vector<int> var_id_to_point_of_last_use;

  /// The set of free/unused spill slots.
  std::bitset<kSpillAreaSize> free_spill_bitmap;

  /// Map variable to the slot it was spilled to (if it is spilled).  
  std::vector<Optional<int>> var_id_to_spill_slot;

  /// Array of currently allocated scratch GPRs.
  std::vector<Xbyak::Reg32> temp_host_gprs;

  /// Array of currently allocated scratch XMM registers.
  std::vector<Xbyak::Xmm> temp_host_xmms;

  /// The current IR program location.
  int location = 0;

  /// Iterator pointing to the current IR program location.
  IREmitter::InstructionList::const_iterator current_op_iter;
};

} // namespace lunatic::backend
} // namespace lunatic
