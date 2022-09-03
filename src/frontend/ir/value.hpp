/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <fmt/format.h>
#include <lunatic/integer.hpp>
#include <stdexcept>

#include "common/optional.hpp"
#include "common/pool_allocator.hpp"

namespace lunatic {
namespace frontend {

enum class IRDataType {
  UInt32,
  SInt32
};

/// Represents an immutable variable
struct IRVariable : PoolObject {
  IRVariable(IRVariable const& other) = delete;

  /// ID that is unique inside the IREmitter instance.
  const u32 id;

  /// The underlying data type
  const IRDataType data_type;

  /// An optional label to hint at the variable usage
  char const* const label;

private:
  friend struct IREmitter;

  IRVariable(
    u32 id,
    IRDataType data_type,
    char const* label
  ) : id(id), data_type(data_type), label(label) {}
};

/// Represents an immediate (constant) value
struct IRConstant {
  IRConstant() : data_type(IRDataType::UInt32), value(0) {}
  IRConstant(u32 value) : data_type(IRDataType::UInt32), value(value) {}

  /// The underlying data type
  IRDataType data_type;

  /// The underlying constant value
  u32 value;
};

/// Represents an IR argument that can be null, a constant or a variable.
struct IRAnyRef {
  IRAnyRef() {}
  IRAnyRef(IRVariable const& variable) : type(Type::Variable), variable(&variable) {}
  IRAnyRef(IRConstant const& constant) : type(Type::Constant), constant( constant) {}

  auto operator=(IRAnyRef const& other) -> IRAnyRef& {
    type = other.type;
    if (IsConstant()) {
      constant = other.constant;
    } else {
      variable = other.variable;
    }
    return *this;
  }

  bool IsNull() const { return type == Type::Null; }
  bool IsVariable() const { return type == Type::Variable; }
  bool IsConstant() const { return type == Type::Constant; }

  auto GetVar() const -> IRVariable const& {
    if (!IsVariable()) {
      throw std::runtime_error("called GetVar() but value is a constant or null");
    }
    return *variable;
  }

  auto GetConst() const -> IRConstant const& {
    if (!IsConstant()) {
      throw std::runtime_error("called GetConst() but value is a variable or null");
    }
    return constant;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) {
    if (IsVariable() && (&GetVar() == &var_old)) {
      variable = &var_new;
    }
  }

  void PropagateConstant(
    IRVariable const& var,
    IRConstant const& constant
  ) {
    if (IsVariable() && (&GetVar() == &var)) {
      type = Type::Constant;
      this->constant = constant;
    }
  }

private:
  enum class Type {
    Null,
    Variable,
    Constant
  };

  Type type = Type::Null;

  union {
    IRVariable const* variable;
    IRConstant constant;
  };
};

/// Represents an IR argument that always is a variable.
struct IRVarRef {
  IRVarRef(IRVariable const& var) : p_var(&var) {}

  auto Get() const -> IRVariable const& {
    return *p_var;
  }

  void Repoint(
    IRVariable const& var_old,
    IRVariable const& var_new
  ) {
    if (&var_old == p_var) {
      p_var = &var_new;
    }
  }

private:
  IRVariable const* p_var;
};

} // namespace lunatic::frontend 
} // namespace lunatic

namespace std {

inline auto to_string(lunatic::frontend::IRDataType data_type) -> std::string {
  switch (data_type) {
    case lunatic::frontend::IRDataType::UInt32:
      return "u32";
    default:
      return "???";
  }
}

inline auto to_string(lunatic::frontend::IRVariable const& variable) -> std::string {
  if (variable.label) {
    return fmt::format("var{}_{}", variable.id, variable.label);
  }
  return fmt::format("var{}", variable.id);
}

inline auto to_string(lunatic::frontend::IRConstant const& constant) -> std::string {
  return fmt::format("0x{:08X}", constant.value);
}

inline auto to_string(lunatic::frontend::IRAnyRef const& value) -> std::string {
  if (value.IsNull()) {
    return "(null)";
  }
  if (value.IsConstant()) {
    return std::to_string(value.GetConst());
  }
  return std::to_string(value.GetVar());
}

inline auto to_string(Optional<lunatic::frontend::IRVariable const&> value) -> std::string {
  if (value.IsNull()) {
    return "(null)";
  }
  return std::to_string(value.Unwrap());
}

inline auto to_string(lunatic::frontend::IRVarRef const& variable) -> std::string {
  return std::to_string(variable.Get());
}

} // namespace std
