/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lunatic/integer.hpp>

#include "arm.hpp"

namespace lunatic {
namespace frontend {

namespace detail {

using ARMDataOp = ARMDataProcessing::Opcode;

enum class ThumbDataOp {
  AND = 0,
  EOR = 1,
  LSL = 2,
  LSR = 3,
  ASR = 4,
  ADC = 5,
  SBC = 6,
  ROR = 7,
  TST = 8,
  NEG = 9,
  CMP = 10,
  CMN = 11,
  ORR = 12,
  MUL = 13,
  BIC = 14,
  MVN = 15
};

enum class ThumbHighRegOp {
  ADD = 0,
  CMP = 1,
  MOV = 2,
  BLX = 3
};

template<typename T, typename U = typename T::return_type>
inline auto decode_move_shifted_register(u16 opcode, T& client) -> U {
  auto info = ARMDataProcessing{};

  info.condition = Condition::AL;
  info.opcode = ARMDataOp::MOV;
  info.immediate = false;
  info.set_flags = true;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  info.op2_reg.reg = bit::get_field<u16, GPR>(opcode, 3, 3);
  info.op2_reg.shift.type = bit::get_field<u16, Shift>(opcode, 11, 2);
  info.op2_reg.shift.immediate = true;
  info.op2_reg.shift.amount_imm = bit::get_field(opcode, 6, 5);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_add_sub(u16 opcode, T& client) -> U {
  bool immediate = bit::get_bit<u16, bool>(opcode, 10);
  auto info = ARMDataProcessing{};

  info.condition = Condition::AL;
  info.opcode = bit::get_bit(opcode, 9) ? ARMDataOp::SUB : ARMDataOp::ADD;
  info.immediate = immediate;
  info.set_flags = true;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  info.reg_op1 = bit::get_field<u16, GPR>(opcode, 3, 3);

  if (immediate) {
    info.op2_imm.value = bit::get_field<u16, u8>(opcode, 6, 3);
    info.op2_imm.shift = 0;
  } else {
    info.op2_reg.reg = bit::get_field<u16, GPR>(opcode, 6, 3);
    info.op2_reg.shift.type = Shift::LSL;
    info.op2_reg.shift.immediate = true;
    info.op2_reg.shift.amount_imm = 0;
  }

  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_mov_cmp_add_sub_imm(u16 opcode, T& client) -> U {
  ARMDataOp op;

  switch (bit::get_field(opcode, 11, 2)) {
    case 0b00: op = ARMDataOp::MOV; break;
    case 0b01: op = ARMDataOp::CMP; break;
    case 0b10: op = ARMDataOp::ADD; break;
    case 0b11: op = ARMDataOp::SUB; break;
  }

  auto info = ARMDataProcessing{};

  info.condition = Condition::AL;
  info.opcode = op;
  info.immediate = true;
  info.set_flags = true;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 8, 3);
  info.reg_op1 = bit::get_field<u16, GPR>(opcode, 8, 3);
  info.op2_imm.value = bit::get_field<u16, u8>(opcode, 0, 8);
  info.op2_imm.shift = 0;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_alu(u16 opcode, T& client) -> U {
  auto op = bit::get_field<u16, ThumbDataOp>(opcode, 6, 4);
  auto reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  auto reg_src = bit::get_field<u16, GPR>(opcode, 3, 3);

  auto decode_dp_alu = [&](ARMDataOp op) {
    auto info = ARMDataProcessing{};

    info.condition = Condition::AL;
    info.opcode = static_cast<ARMDataOp>(op);
    info.immediate = false;
    info.set_flags = true;
    info.reg_dst = reg_dst;
    info.reg_op1 = reg_dst;
    info.op2_reg.reg = reg_src;
    info.op2_reg.shift.type = Shift::LSL;
    info.op2_reg.shift.immediate = true;
    info.op2_reg.shift.amount_imm = 0;
    return client.Handle(info);
  };

  auto decode_dp_shift = [&](Shift type) {
    auto info = ARMDataProcessing{};

    info.condition = Condition::AL;
    info.opcode = ARMDataOp::MOV;
    info.immediate = false;
    info.set_flags = true;
    info.reg_dst = reg_dst;
    info.op2_reg.reg = reg_dst;
    info.op2_reg.shift.type = type;
    info.op2_reg.shift.immediate = false;
    info.op2_reg.shift.amount_reg = reg_src;
    return client.Handle(info);
  };

  auto decode_dp_neg = [&]() {
    auto info = ARMDataProcessing{};

    info.condition = Condition::AL;
    info.opcode = ARMDataOp::RSB;
    info.immediate = true;
    info.set_flags = true;
    info.reg_dst = reg_dst;
    info.reg_op1 = reg_src;
    info.op2_imm.value = 0;
    info.op2_imm.shift = 0;
    return client.Handle(info);
  };

  auto decode_mul = [&]() {
    auto info = ARMMultiply{};

    info.condition = Condition::AL;
    info.accumulate = false;
    info.set_flags = true;
    info.reg_op1 = reg_dst;
    info.reg_op2 = reg_src;
    info.reg_dst = reg_dst;
    return client.Handle(info);
  };

  switch (op) {
    case ThumbDataOp::AND: return decode_dp_alu(ARMDataOp::AND);
    case ThumbDataOp::EOR: return decode_dp_alu(ARMDataOp::EOR);
    case ThumbDataOp::LSL: return decode_dp_shift(Shift::LSL);
    case ThumbDataOp::LSR: return decode_dp_shift(Shift::LSR);
    case ThumbDataOp::ASR: return decode_dp_shift(Shift::ASR);
    case ThumbDataOp::ADC: return decode_dp_alu(ARMDataOp::ADC);
    case ThumbDataOp::SBC: return decode_dp_alu(ARMDataOp::SBC);
    case ThumbDataOp::ROR: return decode_dp_shift(Shift::ROR);
    case ThumbDataOp::TST: return decode_dp_alu(ARMDataOp::TST);
    case ThumbDataOp::NEG: return decode_dp_neg();
    case ThumbDataOp::CMP: return decode_dp_alu(ARMDataOp::CMP);
    case ThumbDataOp::CMN: return decode_dp_alu(ARMDataOp::CMN);
    case ThumbDataOp::ORR: return decode_dp_alu(ARMDataOp::ORR);
    case ThumbDataOp::MUL: return decode_mul();
    case ThumbDataOp::BIC: return decode_dp_alu(ARMDataOp::BIC);
    case ThumbDataOp::MVN: return decode_dp_alu(ARMDataOp::MVN);
  }
}

template<typename T, typename U = typename T::return_type>
inline auto decode_high_register_ops(u16 opcode, T& client) -> U {
  auto op = bit::get_field<u16, ThumbHighRegOp>(opcode, 8, 2);
  auto high1 = bit::get_bit(opcode, 7) * 8;
  auto high2 = bit::get_bit(opcode, 6) * 8;
  auto reg_dst = static_cast<GPR>(bit::get_field(opcode, 0, 3) | high1);
  auto reg_src = static_cast<GPR>(bit::get_field(opcode, 3, 3) | high2);
  auto info_dp = ARMDataProcessing{};
  auto info_bx = ARMBranchExchange{};

  if (op != ThumbHighRegOp::BLX) {
    info_dp.condition = Condition::AL;
    info_dp.immediate = false;
    info_dp.reg_dst = reg_dst;
    info_dp.reg_op1 = reg_dst;
    info_dp.op2_reg.reg = reg_src;
    info_dp.op2_reg.shift.type = Shift::LSL;
    info_dp.op2_reg.shift.immediate = true;
    info_dp.op2_reg.shift.amount_imm = 0;
  }

  switch (op) {
    case ThumbHighRegOp::ADD: {
      info_dp.opcode = ARMDataOp::ADD;
      info_dp.set_flags = false;
      return client.Handle(info_dp);
    }
    case ThumbHighRegOp::CMP: {
      info_dp.opcode = ARMDataOp::CMP;
      info_dp.set_flags = true;
      return client.Handle(info_dp);
    }
    case ThumbHighRegOp::MOV: {
      info_dp.opcode = ARMDataOp::MOV;
      info_dp.set_flags = false;
      return client.Handle(info_dp);
    }
    case ThumbHighRegOp::BLX: {
      info_bx.condition = Condition::AL;
      info_bx.reg = reg_src;
      info_bx.link = high1 != 0;
      return client.Handle(info_bx);
    }
  }
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_relative_pc(u16 opcode, T& client) -> U {
  auto info = ARMSingleDataTransfer{};

  info.condition = Condition::AL;
  info.immediate = true;
  info.pre_increment = true;
  info.add = true;
  info.byte = false;
  info.writeback = false;
  info.load = true;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 8, 3);
  info.reg_base = GPR::PC;
  info.offset_imm = u32(bit::get_field(opcode, 0, 8) * sizeof(u32));
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_store_offset_reg(u16 opcode, T& client) -> U {
  auto info = ARMSingleDataTransfer{};

  info.condition = Condition::AL;
  info.immediate = false;
  info.pre_increment = true;
  info.add = true;
  info.byte = bit::get_bit<u16, bool>(opcode, 10);
  info.writeback = false;
  info.load = bit::get_bit<u16, bool>(opcode, 11);
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  info.reg_base = bit::get_field<u16, GPR>(opcode, 3, 3);
  info.offset_reg.reg = bit::get_field<u16, GPR>(opcode, 6, 3);
  info.offset_reg.shift = Shift::LSL;
  info.offset_reg.amount = 0;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_store_signed(u16 opcode, T& client) -> U {
  auto info = ARMHalfwordSignedTransfer{};

  info.condition = Condition::AL;
  info.pre_increment = true;
  info.add = true;
  info.immediate = false;
  info.writeback = false;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  info.reg_base = bit::get_field<u16, GPR>(opcode, 3, 3);
  info.offset_reg = bit::get_field<u16, GPR>(opcode, 6, 3);

  switch (bit::get_field(opcode, 10, 2)) {
    case 0b00: {
      // STRH
      info.opcode = 1;
      info.load = false;
      break;
    }
    case 0b01: {
      // LDRSB
      info.opcode = 2;
      info.load = true;
      break;
    }
    case 0b10: {
      // LDRH
      info.opcode = 1;
      info.load = true;
      break;
    }
    case 0b11: {
      // LDRSH
      info.opcode = 3;
      info.load = true;
      break;
    }
  }

  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_store_offset_imm(u16 opcode, T& client) -> U {
  auto byte = bit::get_bit<u16, bool>(opcode, 12);
  auto offset = bit::get_field(opcode, 6, 5);

  if (!byte) offset <<= 2;

  auto info = ARMSingleDataTransfer{};

  info.condition = Condition::AL;
  info.immediate = true;
  info.pre_increment = true;
  info.add = true;
  info.byte = byte;
  info.writeback = false;
  info.load = bit::get_bit<u16, bool>(opcode, 11);
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  info.reg_base = bit::get_field<u16, GPR>(opcode, 3, 3);
  info.offset_imm = offset;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_store_half(u16 opcode, T& client) -> U {
  auto info = ARMHalfwordSignedTransfer{};

  info.condition = Condition::AL;
  info.pre_increment = true;
  info.add = true;
  info.immediate = true;
  info.writeback = false;
  info.load = bit::get_bit<u16, bool>(opcode, 11);
  info.opcode = 1;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 0, 3);
  info.reg_base = bit::get_field<u16, GPR>(opcode, 3, 3);
  info.offset_imm = u32(bit::get_field(opcode, 6, 5) << 1);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_store_relative_sp(u16 opcode, T& client) -> U {
  auto info = ARMSingleDataTransfer{};

  info.condition = Condition::AL;
  info.immediate = true;
  info.pre_increment = true;
  info.add = true;
  info.byte = false;
  info.writeback = false;
  info.load = bit::get_bit<u16, bool>(opcode, 11);
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 8, 3);
  info.reg_base = GPR::SP;
  info.offset_imm = u32(bit::get_field(opcode, 0, 8) << 2);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_load_address(u16 opcode, T& client) -> U {
  auto info = ARMDataProcessing{};

  info.condition = Condition::AL;
  info.opcode = ARMDataOp::ADD;
  info.immediate = true;
  info.set_flags = false;
  info.reg_dst = bit::get_field<u16, GPR>(opcode, 8, 3);
  info.reg_op1 = bit::get_bit(opcode, 11) ? GPR::SP : GPR::PC;
  info.op2_imm.value = u32(bit::get_field(opcode, 0, 8) << 2);
  info.op2_imm.shift = 0;
  info.thumb_load_address = true;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_add_sp_offset(u16 opcode, T& client) -> U {
  auto info = ARMDataProcessing{};

  info.condition = Condition::AL;
  info.opcode = bit::get_bit(opcode, 7) ? ARMDataOp::SUB : ARMDataOp::ADD;
  info.immediate = true;
  info.set_flags = false;
  info.reg_dst = GPR::SP;
  info.reg_op1 = GPR::SP;
  info.op2_imm.value = u32(bit::get_field(opcode, 0, 7) << 2);
  info.op2_imm.shift = 0;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_push_pop(u16 opcode, T& client) -> U {
  bool load  = bit::get_bit(opcode, 11);
  auto rlist = bit::get_field(opcode, 0, 8);

  if (bit::get_bit(opcode,  8)) {
    rlist |= (1 << (load ? 15 : 14));
  }

  auto info = ARMBlockDataTransfer{};

  info.condition = Condition::AL;
  info.pre_increment = !load;
  info.add = load;
  info.user_mode = false;
  info.writeback = true;
  info.load = load;
  info.reg_base = GPR::SP;
  info.reg_list = rlist;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_ldm_stm(u16 opcode, T& client) -> U {
  bool writeback = true;
  bool load = bit::get_bit<u16, bool>(opcode, 11);
  auto reg_base = bit::get_field<u16, GPR>(opcode, 8, 3);
  auto reg_list = bit::get_field(opcode, 0, 8);

  if (load && bit::get_bit(reg_list, int(reg_base))) {
    writeback = false;
  }

  auto info = ARMBlockDataTransfer{};

  info.condition = Condition::AL;
  info.pre_increment = false;
  info.add = true;
  info.user_mode = false;
  info.writeback = writeback;
  info.load = load;
  info.reg_base = reg_base;
  info.reg_list = reg_list;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_conditional_branch(u16 opcode, T& client) -> U {
  auto info = ARMBranchRelative{};

  info.condition = bit::get_field<u16, Condition>(opcode, 8, 4);
  info.offset = (bit::get_field<u16, s32>(opcode, 0, 8) << 24) >> 23;
  info.link = false;
  info.exchange = false;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_svc(u16 opcode, T& client) -> U {
  auto info = ARMException{};

  info.condition = Condition::AL;
  info.exception = Exception::Supervisor;
  info.svc_comment = u32(bit::get_field(opcode, 0, 8) << 16);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_unconditional_branch(u16 opcode, T& client) -> U {
  auto info = ARMBranchRelative{};

  info.condition = Condition::AL;
  info.offset = (bit::get_field<u16, s32>(opcode, 0, 11) << 21) >> 20;
  info.link = false;
  info.exchange = false;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_branch_link_prefix(u16 opcode, T& client) -> U {
  auto info = ARMDataProcessing{};

  info.condition = Condition::AL;
  info.opcode = ARMDataOp::ADD;
  info.immediate = true;
  info.set_flags = false;
  info.reg_dst = GPR::LR;
  info.reg_op1 = GPR::PC;
  info.op2_imm.value = u32((bit::get_field<u16, s32>(opcode, 0, 11) << 21) >> 9);
  info.op2_imm.shift = 0;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_branch_link_suffix(u16 opcode, T& client, bool exchange) -> U {
  auto info = ThumbBranchLinkSuffix{};

  info.offset = bit::get_field<u16, u32>(opcode, 0, 11) << 1;
  info.exchange = exchange;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_branch_link_full(u32 opcode, T& client) -> U {
  auto offset = bit::get_field<u32, s32>(opcode, 16, 11) << 1;

  offset += (bit::get_field<u32, s32>(opcode, 0, 11) << 21) >> 9;

  auto info = ARMBranchRelative{};

  info.condition = Condition::AL;
  info.offset = offset;
  info.link = true;
  info.exchange = !bit::get_bit<u32, bool>(opcode, 28);
  return client.Handle(info);
}

} // namespace lunatic::frontend::detail

/// Decodes a Thumb opcode into one of multiple structures,
/// passes the resulting structure to a client and returns the client's return value.
template<typename T, typename U = typename T::return_type>
inline auto decode_thumb(u32 opcode, T& client) -> U {
  using namespace detail;

  if ((opcode & 0xE800'F800) == 0xE800'F000) {
    return decode_branch_link_full(opcode, client);
  }

  if ((opcode & 0xF800) <  0x1800) return decode_move_shifted_register(opcode, client);
  if ((opcode & 0xF800) == 0x1800) return decode_add_sub(opcode, client);
  if ((opcode & 0xE000) == 0x2000) return decode_mov_cmp_add_sub_imm(opcode, client);
  if ((opcode & 0xFC00) == 0x4000) return decode_alu(opcode, client);
  if ((opcode & 0xFC00) == 0x4400) return decode_high_register_ops(opcode, client);
  if ((opcode & 0xF800) == 0x4800) return decode_load_relative_pc(opcode, client);
  if ((opcode & 0xF200) == 0x5000) return decode_load_store_offset_reg(opcode, client);
  if ((opcode & 0xF200) == 0x5200) return decode_load_store_signed(opcode, client);
  if ((opcode & 0xE000) == 0x6000) return decode_load_store_offset_imm(opcode, client);
  if ((opcode & 0xF000) == 0x8000) return decode_load_store_half(opcode, client);
  if ((opcode & 0xF000) == 0x9000) return decode_load_store_relative_sp(opcode, client);
  if ((opcode & 0xF000) == 0xA000) return decode_load_address(opcode, client);
  if ((opcode & 0xFF00) == 0xB000) return decode_add_sp_offset(opcode, client);
  if ((opcode & 0xF600) == 0xB400) return decode_push_pop(opcode, client);
//if ((opcode & 0xFF00) == 0xBE00) return ThumbInstrType::SoftwareBreakpoint;
  if ((opcode & 0xF000) == 0xC000) return decode_ldm_stm(opcode, client);
  if ((opcode & 0xFF00) <  0xDF00) return decode_conditional_branch(opcode, client);
  if ((opcode & 0xFF00) == 0xDF00) return decode_svc(opcode, client);
  if ((opcode & 0xF800) == 0xE000) return decode_unconditional_branch(opcode, client);
  if ((opcode & 0xF800) == 0xE800) return decode_branch_link_suffix(opcode, client, true);
  if ((opcode & 0xF800) == 0xF000) return decode_branch_link_prefix(opcode, client);
  if ((opcode & 0xF800) == 0xF800) return decode_branch_link_suffix(opcode, client, false);

  return client.Undefined(opcode); // TODO: distinguish ARM and Thumb opcodes.
}

} // namespace lunatic::frontend
} // namespace lunatic
