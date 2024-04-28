/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdexcept>

#include "state.hpp"

namespace lunatic::frontend {

  State::State() {
    InitializeLookupTable();
    Reset();
  }

  void State::Reset() {
    m_common = {};
    m_fiq = {};
    m_sys = {};
    m_irq = {};
    m_svc = {};
    m_abt = {};
    m_und = {};
  }

  u32* State::GetPointerToGPR(Mode mode, GPR reg) {
    auto id = static_cast<int>(reg);
    if (id > 15) {
      throw std::runtime_error("GetPointerToGPR: requirement not met: 'id' must be <= 15.");
    }
    auto pointer = m_table[static_cast<int>(mode)].gpr[id];
    if (pointer == nullptr) {
      throw std::runtime_error(
        "GetPointerToGPR: requirement not met: 'mode' must be valid ARM processor mode.");
    }
    return pointer;
  }

  StatusRegister* State::GetPointerToSPSR(Mode mode) {
    auto pointer = m_table[static_cast<int>(mode)].spsr;
    if (pointer == nullptr) {
      throw std::runtime_error(
        "GetPointerToSPSR: requirement not met: 'mode' must be valid ARM processor mode "
        "and may not be System or User mode.");
    }
    return pointer;
  }

  StatusRegister* State::GetPointerToCPSR() {
    return &m_common.cpsr;
  }

  uintptr State::GetOffsetToSPSR(Mode mode) {
    return uintptr(GetPointerToSPSR(mode)) - uintptr(this);
  }

  uintptr State::GetOffsetToCPSR() {
    return uintptr(GetPointerToCPSR()) - uintptr(this);
  }

  uintptr State::GetOffsetToGPR(Mode mode, GPR reg) {
    return uintptr(GetPointerToGPR(mode, reg)) - uintptr(this);
  }

  void State::InitializeLookupTable() {
    Mode modes[] = {
      Mode::User,
      Mode::FIQ,
      Mode::IRQ,
      Mode::Supervisor,
      Mode::Abort,
      Mode::Undefined,
      Mode::System
    };

    for (auto mode : modes) {
      for (int i = 0; i <= 7; i++)
        m_table[static_cast<int>(mode)].gpr[i] = &m_common.reg[i];
      auto source = mode == Mode::FIQ ? m_fiq.reg : m_sys.reg;
      for (int i = 8; i <= 12; i++)
        m_table[static_cast<int>(mode)].gpr[i] = &source[i - 8];
      m_table[static_cast<int>(mode)].gpr[15] = &m_common.r15;
    }

    for (int i = 0; i < 2; i++) {
      m_table[static_cast<int>(Mode::User)].gpr[13 + i] = &m_sys.reg[5 + i];
      m_table[static_cast<int>(Mode::FIQ)].gpr[13 + i] = &m_fiq.reg[5 + i];
      m_table[static_cast<int>(Mode::IRQ)].gpr[13 + i] = &m_irq.reg[i];
      m_table[static_cast<int>(Mode::Supervisor)].gpr[13 + i] = &m_svc.reg[i];
      m_table[static_cast<int>(Mode::Abort)].gpr[13 + i] = &m_abt.reg[i];
      m_table[static_cast<int>(Mode::Undefined)].gpr[13 + i] = &m_und.reg[i];
      m_table[static_cast<int>(Mode::System)].gpr[13 + i] = &m_sys.reg[5 + i];
    }

    m_table[static_cast<int>(Mode::User)].spsr = nullptr;
    m_table[static_cast<int>(Mode::FIQ)].spsr = &m_fiq.spsr;
    m_table[static_cast<int>(Mode::IRQ)].spsr = &m_irq.spsr;
    m_table[static_cast<int>(Mode::Supervisor)].spsr = &m_svc.spsr;
    m_table[static_cast<int>(Mode::Abort)].spsr = &m_abt.spsr;
    m_table[static_cast<int>(Mode::Undefined)].spsr = &m_und.spsr;
    m_table[static_cast<int>(Mode::System)].spsr = nullptr;
  }

} // namespace lunatic::frontend
