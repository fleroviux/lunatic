/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <fmt/format.h>
#include <stdexcept>

#include "common/pool_allocator.hpp"
#include "register.hpp"
#include "value.hpp"

namespace lunatic {
namespace frontend {

enum class IROpcodeClass {
  NOP,
  LoadGPR,
  StoreGPR,
  LoadSPSR,
  StoreSPSR,
  LoadCPSR,
  StoreCPSR,
  ClearCarry,
  SetCarry,
  UpdateFlags,
  UpdateSticky,
  LSL,
  LSR,
  ASR,
  ROR,
  AND,
  BIC,
  EOR,
  SUB,
  RSB,
  ADD,
  ADC,
  SBC,
  RSC,
  ORR,
  MOV,
  MVN,
  MUL,
  ADD64,
  MemoryRead,
  MemoryWrite,
  Flush,
  FlushExchange,
  CLZ,
  QADD,
  QSUB,
  MRC,
  MCR
};

// TODO: Reads(), Writes() and ToString() should be const,
// but due to the nature of this Optional<T> implementation this is not possible at the moment.

struct IROpcode : PoolObject {
  virtual ~IROpcode() = default;

  virtual auto GetClass() const -> IROpcodeClass = 0;
  virtual auto Reads (IRVariable const& var) -> bool = 0;
  virtual auto Writes(IRVariable const& var) -> bool = 0;
  virtual void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) = 0;
  virtual void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {}
  virtual auto ToString() -> std::string = 0;
};

template<IROpcodeClass _klass>
struct IROpcodeBase : IROpcode {
  static constexpr IROpcodeClass klass = _klass;

  auto GetClass() const -> IROpcodeClass override { return _klass; }
};

struct IRNoOp final : IROpcodeBase<IROpcodeClass::NOP> {
  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
  }

  auto ToString() -> std::string override {
    return "nop";
  }
};

struct IRLoadGPR final : IROpcodeBase<IROpcodeClass::LoadGPR> {
  IRLoadGPR(
    IRGuestReg reg,
    IRVariable const& result
  )   : reg(reg), result(result) {}

  IRGuestReg reg;
  IRVarRef result;

  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "ldgpr {}, {}",
      std::to_string(reg),
      std::to_string(result)
    );
  }
};

struct IRStoreGPR final : IROpcodeBase<IROpcodeClass::StoreGPR> {
  IRStoreGPR(
    IRGuestReg reg,
    IRAnyRef value
  )   : reg(reg), value(value) {}

  IRGuestReg reg;
  IRAnyRef value;

  auto Reads(IRVariable const& var) -> bool override {
    if (value.IsVariable()) {
      return &var == &value.GetVar();
    }
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    value.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    value.PropagateConstant(var, constant);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "stgpr {}, {}",
      std::to_string(reg),
      std::to_string(value)
    );
  }
};

struct IRLoadSPSR final : IROpcodeBase<IROpcodeClass::LoadSPSR> {
  IRLoadSPSR(
    IRVariable const& result,
    Mode mode
  )   : result(result), mode(mode) {}

  IRVarRef result;
  Mode mode;

  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "ldspsr.{} {}",
      std::to_string(mode),
      std::to_string(result)
    );
  }
};

struct IRStoreSPSR final : IROpcodeBase<IROpcodeClass::StoreSPSR> {
  IRStoreSPSR(
    IRAnyRef value,
    Mode mode
  )   : value(value), mode(mode) {}

  IRAnyRef value;
  Mode mode;

  auto Reads(IRVariable const& var) -> bool override {
    return value.IsVariable() && (&var == &value.GetVar());
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    value.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    value.PropagateConstant(var, constant);  
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "stspsr.{} {}",
      std::to_string(mode),
      std::to_string(value)
    );
  }
};

struct IRLoadCPSR final : IROpcodeBase<IROpcodeClass::LoadCPSR> {
  IRLoadCPSR(IRVariable const& result)
      : result(result) {}

  IRVarRef result;

  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format("ldcpsr {}", std::to_string(result));
  }
};

struct IRStoreCPSR final : IROpcodeBase<IROpcodeClass::StoreCPSR> {
  IRStoreCPSR(IRAnyRef value) : value(value) {}

  IRAnyRef value;

  auto Reads(IRVariable const& var) -> bool override {
    return value.IsVariable() && &var == &value.GetVar();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    value.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    value.PropagateConstant(var, constant);  
  }

  auto ToString() -> std::string override {
    return fmt::format("stcpsr {}", std::to_string(value));
  }
};

struct IRClearCarry final : IROpcodeBase<IROpcodeClass::ClearCarry> {
  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    (void)var_old;
    (void)var_new;
  }

  auto ToString() -> std::string override {
    return "clearcarry";
  }
};

struct IRSetCarry final : IROpcodeBase<IROpcodeClass::SetCarry> {
  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    (void)var_old;
    (void)var_new;
  }

  auto ToString() -> std::string override {
    return "setcarry";
  }
};

struct IRUpdateFlags final : IROpcodeBase<IROpcodeClass::UpdateFlags> {
  IRUpdateFlags(
    IRVariable const& result,
    IRVariable const& input,
    bool flag_n,
    bool flag_z,
    bool flag_c,
    bool flag_v
  )   : result(result)
      , input(input)
      , flag_n(flag_n)
      , flag_z(flag_z)
      , flag_c(flag_c)
      , flag_v(flag_v) {
  }

  IRVarRef result;
  IRVarRef input;
  bool flag_n;
  bool flag_z;
  bool flag_c;
  bool flag_v;

  auto Reads(IRVariable const& var) -> bool override {
    return &input.Get() == &var;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &result.Get() == &var;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    input.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "update.{}{}{}{} {}, {}",
      flag_n ? 'n' : '-',
      flag_z ? 'z' : '-',
      flag_c ? 'c' : '-',
      flag_v ? 'v' : '-',
      std::to_string(result),
      std::to_string(input)
    );
  }
};

struct IRUpdateSticky final : IROpcodeBase<IROpcodeClass::UpdateSticky> {
  IRUpdateSticky(
    IRVariable const& result,
    IRVariable const& input
  )   : result(result), input(input) {}

  IRVarRef result;
  IRVarRef input;

  auto Reads(IRVariable const& var) -> bool override {
    return &input.Get() == &var;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &result.Get() == &var;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    input.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "update.q {}, {}",
      std::to_string(result),
      std::to_string(input)
    );
  }
};

template<IROpcodeClass _klass>
struct IRShifterBase : IROpcodeBase<_klass> {
  IRShifterBase(
    IRVariable const& result,
    IRVariable const& operand,
    IRAnyRef amount,
    bool update_host_flags
  )   : result(result)
      , operand(operand)
      , amount(amount)
      , update_host_flags(update_host_flags) {
  }

  IRVarRef result;
  IRVarRef operand;
  IRAnyRef amount;
  bool update_host_flags;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &operand.Get() ||
          (amount.IsVariable() && &var == &amount.GetVar());
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    operand.Repoint(var_old, var_new);
    amount.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    // TODO: this is unsafe, because shifter behaviour is different for
    // shift-by-register vs shift-by-immediate instructions.
    //amount.PropagateConstant(var, constant);
  }
};

struct IRLogicalShiftLeft final : IRShifterBase<IROpcodeClass::LSL> {
  using IRShifterBase::IRShifterBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "lsl{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(operand),
      std::to_string(amount)
    );
  }
};

struct IRLogicalShiftRight final : IRShifterBase<IROpcodeClass::LSR> {
  using IRShifterBase::IRShifterBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "lsr{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(operand),
      std::to_string(amount)
    );
  }
};

struct IRArithmeticShiftRight final : IRShifterBase<IROpcodeClass::ASR> {
  using IRShifterBase::IRShifterBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "asr{} {}, {}, {}",
      update_host_flags ? "s": "",
      std::to_string(result),
      std::to_string(operand),
      std::to_string(amount)
    );
  }
};

struct IRRotateRight final : IRShifterBase<IROpcodeClass::ROR> {
  using IRShifterBase::IRShifterBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "ror{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(operand),
      std::to_string(amount)
    );
  }
};

template<IROpcodeClass _klass>
struct IRBinaryOpBase : IROpcodeBase<_klass> {
  IRBinaryOpBase(
    Optional<IRVariable const&> result,
    IRVariable const& lhs,
    IRAnyRef rhs,
    bool update_host_flags
  )   : result(result)
      , lhs(lhs)
      , rhs(rhs)
      , update_host_flags(update_host_flags) {
  }

  Optional<IRVariable const&> result;
  IRVarRef lhs;
  IRAnyRef rhs;
  bool update_host_flags;

  auto Reads(IRVariable const& var) -> bool override {
    return &lhs.Get() == &var || 
          (rhs.IsVariable() && (&rhs.GetVar() == &var));
  }

  auto Writes(IRVariable const& var) -> bool override {
    return result.HasValue() && (&result.Unwrap() == &var);
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    // TODO: make this reusable?
    if (result.HasValue() && (&result.Unwrap() == &var_old)) {
      result = var_new;
    }

    lhs.Repoint(var_old, var_new);
    rhs.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    rhs.PropagateConstant(var, constant);  
  }
};

struct IRBitwiseAND final : IRBinaryOpBase<IROpcodeClass::AND> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "and{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRBitwiseBIC final : IRBinaryOpBase<IROpcodeClass::BIC> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "bic{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRBitwiseEOR final : IRBinaryOpBase<IROpcodeClass::EOR> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "eor{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRSub final : IRBinaryOpBase<IROpcodeClass::SUB> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "sub{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRRsb final : IRBinaryOpBase<IROpcodeClass::RSB> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "rsb{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRAdd final : IRBinaryOpBase<IROpcodeClass::ADD> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "add{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRAdc final : IRBinaryOpBase<IROpcodeClass::ADC> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "adc{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRSbc final : IRBinaryOpBase<IROpcodeClass::SBC> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "sbc{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRRsc final : IRBinaryOpBase<IROpcodeClass::RSC> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "rsc{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRBitwiseORR final : IRBinaryOpBase<IROpcodeClass::ORR> {
  using IRBinaryOpBase::IRBinaryOpBase;

  auto ToString() -> std::string override {
    return fmt::format(
      "orr{} {}, {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRMov final : IROpcodeBase<IROpcodeClass::MOV> {
  IRMov(
    IRVariable const& result,
    IRAnyRef source,
    bool update_host_flags
  )   : result(result)
      , source(source)
      , update_host_flags(update_host_flags) {
  }

  IRVarRef result;
  IRAnyRef source;
  bool update_host_flags;

  auto Reads(IRVariable const& var) -> bool override {
    return source.IsVariable() && (&source.GetVar() == &var);
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &result.Get() == &var;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    source.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    source.PropagateConstant(var, constant);  
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "mov{} {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(source)
    );
  }
};

struct IRMvn final : IROpcodeBase<IROpcodeClass::MVN> {
  IRMvn(
    IRVariable const& result,
    IRAnyRef source,
    bool update_host_flags
  )   : result(result)
      , source(source)
      , update_host_flags(update_host_flags) {
  }

  IRVarRef result;
  IRAnyRef source;
  bool update_host_flags;

  auto Reads(IRVariable const& var) -> bool override {
    return source.IsVariable() && (&source.GetVar() == &var);
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &result.Get() == &var;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    source.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    source.PropagateConstant(var, constant);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "mvn{} {}, {}",
      update_host_flags ? "s" : "",
      std::to_string(result),
      std::to_string(source)
    );
  }
};

struct IRMultiply final : IROpcodeBase<IROpcodeClass::MUL> {
  IRMultiply(
    Optional<IRVariable const&> result_hi,
    IRVariable const& result_lo,
    IRVariable const& lhs,
    IRVariable const& rhs,
    bool update_host_flags
  )   : result_hi(result_hi)
      , result_lo(result_lo)
      , lhs(lhs)
      , rhs(rhs)
      , update_host_flags(update_host_flags) {
  }

  Optional<IRVariable const&> result_hi;
  IRVarRef result_lo;
  IRVarRef lhs;
  IRVarRef rhs;
  bool update_host_flags;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &lhs.Get() || &var == &rhs.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result_lo.Get() || 
          (result_hi.HasValue() && (&result_hi.Unwrap() == &var));
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    if (result_hi.HasValue() && (&result_hi.Unwrap() == &var_old)) {
      result_hi = var_new;
    }

    result_lo.Repoint(var_old, var_new);
    lhs.Repoint(var_old, var_new);
    rhs.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    std::string result_str;

    if (result_hi.HasValue()) {
      result_str = fmt::format(
        "({}, {})",
        std::to_string(result_hi.Unwrap()),
        std::to_string(result_lo)
      );
    } else {
      result_str = std::to_string(result_lo);
    }

    return fmt::format(
      "mul{} {}, {}, {}",
      update_host_flags ? "s" : "",
      result_str,
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRAdd64 final : IROpcodeBase<IROpcodeClass::ADD64> {
  IRAdd64(
    IRVariable const& result_hi,
    IRVariable const& result_lo,
    IRVariable const& lhs_hi,
    IRVariable const& lhs_lo,
    IRVariable const& rhs_hi,
    IRVariable const& rhs_lo,
    bool update_host_flags
  )   : result_hi(result_hi)
      , result_lo(result_lo)
      , lhs_hi(lhs_hi)
      , lhs_lo(lhs_lo)
      , rhs_hi(rhs_hi)
      , rhs_lo(rhs_lo)
      , update_host_flags(update_host_flags) {
  }

  IRVarRef result_hi;
  IRVarRef result_lo;
  IRVarRef lhs_hi;
  IRVarRef lhs_lo;
  IRVarRef rhs_hi;
  IRVarRef rhs_lo;
  bool update_host_flags;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &lhs_hi.Get() ||
           &var == &lhs_lo.Get() ||
           &var == &rhs_hi.Get() ||
           &var == &rhs_lo.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result_hi.Get() || &var == &result_lo.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result_hi.Repoint(var_old, var_new);
    result_lo.Repoint(var_old, var_new);
    lhs_hi.Repoint(var_old, var_new);
    lhs_lo.Repoint(var_old, var_new);
    rhs_hi.Repoint(var_old, var_new);
    rhs_lo.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "add{} ({}, {}), ({}, {}), ({}, {})",
      update_host_flags ? "s": "",
      std::to_string(result_hi),
      std::to_string(result_lo),
      std::to_string(lhs_hi),
      std::to_string(lhs_lo),
      std::to_string(rhs_hi),
      std::to_string(rhs_lo)
    );
  }
};

enum IRMemoryFlags {
  Byte = 1,
  Half = 2,
  Word = 4,
  Rotate = 8,
  Signed = 16,
  ARMv4T = 32
};

constexpr auto operator|(IRMemoryFlags lhs, IRMemoryFlags rhs) -> IRMemoryFlags {
  return static_cast<IRMemoryFlags>(int(lhs) | rhs);
}

struct IRMemoryRead final : IROpcodeBase<IROpcodeClass::MemoryRead> {
  IRMemoryRead(
    IRMemoryFlags flags,
    IRVariable const& result,
    IRAnyRef address
  )   : flags(flags)
      , result(result)
      , address(address) {
  }

  IRMemoryFlags flags;
  IRVarRef result;
  IRAnyRef address;

  auto Reads(IRVariable const& var) -> bool override {
    return address.IsVariable() && (&address.GetVar() == &var);
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &result.Get() == &var;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    address.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) override {
    address.PropagateConstant(var, constant);
  }

  auto ToString() -> std::string override {
    auto size = "b";

    if (flags & IRMemoryFlags::Half) size = "h";
    if (flags & IRMemoryFlags::Word) size = "w";

    return fmt::format(
      "ldr.{}{} {}, [{}]",
      size,
      (flags & IRMemoryFlags::Rotate) ? "r" : "",
      std::to_string(result),
      std::to_string(address)
    );
  }
};

struct IRMemoryWrite final : IROpcodeBase<IROpcodeClass::MemoryWrite> {
  IRMemoryWrite(
    IRMemoryFlags flags,
    IRAnyRef source,
    IRAnyRef address
  )   : flags(flags)
      , source(source)
      , address(address) {
  }

  IRMemoryFlags flags;
  IRAnyRef source;
  IRAnyRef address;

  auto Reads(IRVariable const& var) -> bool override {
    return (address.IsVariable() && (&address.GetVar() == &var)) ||
           (source.IsVariable()  && (&source.GetVar()  == &var));
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    source.Repoint(var_old, var_new);
    address.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) override {
    source.PropagateConstant(var, constant);
    address.PropagateConstant(var, constant);
  }

  auto ToString() -> std::string override {
    auto size = "b";

    if (flags & IRMemoryFlags::Half) size = "h";
    if (flags & IRMemoryFlags::Word) size = "w";

    return fmt::format(
      "str.{} {}, [{}]",
      size,
      std::to_string(source),
      std::to_string(address)
    );
  }
};

struct IRFlush final : IROpcodeBase<IROpcodeClass::Flush> {
  IRFlush(
    IRVariable const& address_out,
    IRVariable const& address_in,
    IRVariable const& cpsr_in
  )   : address_out(address_out)
      , address_in(address_in)
      , cpsr_in(cpsr_in) {
  }

  IRVarRef address_out;
  IRVarRef address_in;
  IRVarRef cpsr_in;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &address_in.Get() || &var == &cpsr_in.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &address_out.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    address_out.Repoint(var_old, var_new);
    address_in.Repoint(var_old, var_new);
    cpsr_in.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "flush {}, {}, {}",
      std::to_string(address_out),
      std::to_string(address_in),
      std::to_string(cpsr_in)
    );
  }
};

struct IRFlushExchange final : IROpcodeBase<IROpcodeClass::FlushExchange> {
  IRFlushExchange(
    IRVariable const& address_out,
    IRVariable const& cpsr_out,
    IRVariable const& address_in,
    IRVariable const& cpsr_in
  )   : address_out(address_out)
      , cpsr_out(cpsr_out)
      , address_in(address_in)
      , cpsr_in(cpsr_in) {
  }

  IRVarRef address_out;
  IRVarRef cpsr_out;
  IRVarRef address_in;
  IRVarRef cpsr_in;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &address_in.Get() || &var == &cpsr_in.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &address_out.Get() || &var == &cpsr_out.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    address_out.Repoint(var_old, var_new);
    cpsr_out.Repoint(var_old, var_new);
    address_in.Repoint(var_old, var_new);
    cpsr_in.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "flushxchg {}, {}, {}, {}",
      std::to_string(address_out),
      std::to_string(cpsr_out),
      std::to_string(address_in),
      std::to_string(cpsr_in)
    );
  }
};

struct IRCountLeadingZeros final : IROpcodeBase<IROpcodeClass::CLZ> {
  IRCountLeadingZeros(
    IRVariable const& result,
    IRVariable const& operand
  )   : result(result), operand(operand) {}

  IRVarRef result;
  IRVarRef operand;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &operand.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    operand.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "clz {}, {}",
      std::to_string(result),
      std::to_string(operand)
    );
  }
};

struct IRSaturatingAdd final : IROpcodeBase<IROpcodeClass::QADD> {
  IRSaturatingAdd(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  )   : result(result), lhs(lhs), rhs(rhs) {}

  IRVarRef result;
  IRVarRef lhs;
  IRVarRef rhs;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &lhs.Get() || &var == &rhs.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    lhs.Repoint(var_old, var_new);
    rhs.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "qadd {}, {}, {}",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRSaturatingSub final : IROpcodeBase<IROpcodeClass::QSUB> {
  IRSaturatingSub(
    IRVariable const& result,
    IRVariable const& lhs,
    IRVariable const& rhs
  ) : result(result), lhs(lhs), rhs(rhs) {}

  IRVarRef result;
  IRVarRef lhs;
  IRVarRef rhs;

  auto Reads(IRVariable const& var) -> bool override {
    return &var == &lhs.Get() || &var == &rhs.Get();
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
    lhs.Repoint(var_old, var_new);
    rhs.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "qsub {}, {}, {}",
      std::to_string(result),
      std::to_string(lhs),
      std::to_string(rhs)
    );
  }
};

struct IRReadCoprocessorRegister final : IROpcodeBase<IROpcodeClass::MRC> {
  IRReadCoprocessorRegister(
    IRVariable const& result,
    uint coprocessor_id,
    uint opcode1,
    uint cn,
    uint cm,
    uint opcode2
  )   : result(result)
      , coprocessor_id(coprocessor_id)
      , opcode1(opcode1)
      , cn(cn)
      , cm(cm)
      , opcode2(opcode2) {
  }

  IRVarRef result;
  uint coprocessor_id;
  uint opcode1;
  uint cn;
  uint cm;
  uint opcode2;

  auto Reads(IRVariable const& var) -> bool override {
    return false;
  }

  auto Writes(IRVariable const& var) -> bool override {
    return &var == &result.Get();
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    result.Repoint(var_old, var_new);
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "mrc {}, cp{}, #{}, {}, {}, #{}",
      std::to_string(result),
      coprocessor_id,
      opcode1,
      cn,
      cm,
      opcode2
    );
  }
};

struct IRWriteCoprocessorRegister final : IROpcodeBase<IROpcodeClass::MCR> {
  IRWriteCoprocessorRegister(
    IRAnyRef value,
    uint coprocessor_id,
    uint opcode1,
    uint cn,
    uint cm,
    uint opcode2
  )   : value(value)
      , coprocessor_id(coprocessor_id)
      , opcode1(opcode1)
      , cn(cn)
      , cm(cm)
      , opcode2(opcode2) {
  }

  IRAnyRef value;
  uint coprocessor_id;
  uint opcode1;
  uint cn;
  uint cm;
  uint opcode2;

  auto Reads(IRVariable const& var) -> bool override {
    return value.IsVariable() && (&value.GetVar() == &var);
  }

  auto Writes(IRVariable const& var) -> bool override {
    return false;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) override {
    value.Repoint(var_old, var_new);
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    value.PropagateConstant(var, constant); 
  }

  auto ToString() -> std::string override {
    return fmt::format(
      "mcr {}, cp{}, #{}, {}, {}, #{}",
      std::to_string(value),
      coprocessor_id,
      opcode1,
      cn,
      cm,
      opcode2
    );
  }
};

} // namespace lunatic::frontend
} // namespace lunatic

// TODO: consider that T should be a pointer type.
template<typename T>
auto lunatic_cast(lunatic::frontend::IROpcode* value) -> T* {
  if (value->GetClass() != T::klass) {
    throw std::runtime_error("bad lunatic_cast");
  }

  return reinterpret_cast<T*>(value);
}
