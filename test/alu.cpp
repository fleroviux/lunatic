/*
 * Copyright (C) 2024 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fmt/format.h>
#include <lunatic/cpu.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>

using namespace lunatic;

const u32 sample_count = 1000;

struct TestCodeMemory final : public lunatic::Memory {
  TestCodeMemory() {
    b32.fill(0xE3'20'F0'00); // nop
  }

  auto ReadByte(u32 address, Bus bus) -> u8 override { return b8[address]; }
  auto ReadHalf(u32 address, Bus bus) -> u16 override {
    assert(address % 2 == 0);
    return b16.at(address / 2);
  }
  auto ReadWord(u32 address, Bus bus) -> u32 override {
    assert(address % 4 == 0);
    return b32.at(address / 4);
  }

  void WriteByte(u32 address, u8 value, Bus bus) override {
    b8.at(address) = value;
  }
  void WriteHalf(u32 address, u16 value, Bus bus) override {
    assert(address % 2 == 0);
    b16.at(address / 2) = value;
  }
  void WriteWord(u32 address, u32 value, Bus bus) override {
    assert(address % 4 == 0);
    b32.at(address / 4) = value;
  }

  union {
    std::array<u8, 128> b8;
    std::array<u16, 64> b16;
    std::array<u32, 32> b32;
  };
};

TEST_CASE("AND", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'00'00'01, Memory::Bus::Code); // and r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA & OperandB));
  }
}

TEST_CASE("BIC", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'C0'00'01, Memory::Bus::Code); // bic r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA & ~OperandB));
  }
}

TEST_CASE("EOR", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'20'00'01, Memory::Bus::Code); // eor r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA ^ OperandB));
  }
}

TEST_CASE("SUB", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'40'00'01, Memory::Bus::Code); // sub r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA - OperandB));
  }
}

TEST_CASE("RSB", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'60'00'01, Memory::Bus::Code); // rsb r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandB - OperandA));
  }
}

TEST_CASE("ADD", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'80'00'01, Memory::Bus::Code); // add r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA + OperandB));
  }
}

TEST_CASE("ADC", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'A0'00'01, Memory::Bus::Code); // adc r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomC = Catch::Generators::random<u32>(0, 1);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();
    const u32 OperandC = RandomC.get();
    RandomC.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    StatusRegister CPSR = jit->GetCPSR();
    CPSR.f.c = OperandC;
    jit->SetCPSR(CPSR);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA + OperandB + OperandC));
  }
}

TEST_CASE("SBC", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'C0'00'01, Memory::Bus::Code); // sbc r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomC = Catch::Generators::random<u32>(0, 1);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();
    const u32 OperandC = RandomC.get();
    RandomC.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    StatusRegister CPSR = jit->GetCPSR();
    CPSR.f.c = OperandC;
    jit->SetCPSR(CPSR);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) ==
            u32((OperandA - OperandB) - (OperandC ? 0 : 1)));
  }
}

TEST_CASE("RSC", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'E0'00'01, Memory::Bus::Code); // sbc r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomC = Catch::Generators::random<u32>(0, 1);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();
    const u32 OperandC = RandomC.get();
    RandomC.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    StatusRegister CPSR = jit->GetCPSR();
    CPSR.f.c = OperandC;
    jit->SetCPSR(CPSR);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) ==
            u32((OperandB - OperandA) - (OperandC ? 0 : 1)));
  }
}

TEST_CASE("ORR", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'80'00'01, Memory::Bus::Code); // orr r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA | OperandB));
  }
}

TEST_CASE("MOV", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'A0'00'01, Memory::Bus::Code); // mov r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R1, OperandA);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == OperandA);
  }
}

TEST_CASE("MVN", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'E0'00'01, Memory::Bus::Code); // mov r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R1, OperandA);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == ~OperandA);
  }
}

u32 clz(u32 x) {
  if (x == 0u) {
    return 32;
  }
  u32 count = 0;
  u32 bits = 32;
  for (u32 i = bits / 2; i > 0; i /= 2) {
    u32 mask = 0xFFFFFFFF << (bits - i);
    if ((x & mask) == 0) {
      count += i;
      x <<= i;
    }
  }
  return count;
}

TEST_CASE("CLZ", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'6F'0F'11, Memory::Bus::Code); // clz r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R1, OperandA);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == clz(OperandA));
  }
}

s32 saturated_add(s32 a, s32 b) {
  if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) {
    return (b > 0) ? INT32_MAX : INT32_MIN;
  }
  return a + b;
}

TEST_CASE("QADD", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'01'00'50, Memory::Bus::Code); // qadd r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<s32>(INT32_MIN, INT32_MAX);
  auto RandomB = Catch::Generators::random<s32>(INT32_MIN, INT32_MAX);

  for (u32 i = 0; i < sample_count; ++i) {

    const s32 OperandA = RandomA.get();
    RandomA.next();
    const s32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(s32(jit->GetGPR(GPR::R0)) == saturated_add(OperandA, OperandB));
  }
}

s32 saturated_sub(s32 a, s32 b) {
  s64 result = s64(a) - s64(b);
  if (result < INT_MIN) {
    result = INT_MIN;
  } else if (result > INT_MAX) {
    result = INT_MAX;
  }
  return s32(result);
}

TEST_CASE("QSUB", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE1'21'00'50, Memory::Bus::Code); // qsub r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<s32>(INT32_MIN, INT32_MAX);
  auto RandomB = Catch::Generators::random<s32>(INT32_MIN, INT32_MAX);

  for (u32 i = 0; i < sample_count; ++i) {

    const s32 OperandA = RandomA.get();
    RandomA.next();
    const s32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(s32(jit->GetGPR(GPR::R0)) == saturated_sub(OperandA, OperandB));
  }
}

TEST_CASE("MUL", "[ALU]") {
  TestCodeMemory test_code;
  test_code.WriteWord(0, 0xE0'00'01'90, Memory::Bus::Code); // mul r0, r0, r1
  test_code.WriteWord(4, 0xEA'FF'FF'FE, Memory::Bus::Code); // b +#0

  auto jit = CreateCPU(CPU::Descriptor{test_code});

  auto RandomA = Catch::Generators::random<u32>(0, 0xFFFFFFFF);
  auto RandomB = Catch::Generators::random<u32>(0, 0xFFFFFFFF);

  for (u32 i = 0; i < sample_count; ++i) {

    const u32 OperandA = RandomA.get();
    RandomA.next();
    const u32 OperandB = RandomB.get();
    RandomB.next();

    jit->SetGPR(GPR::PC, 0);
    jit->SetGPR(GPR::R0, OperandA);
    jit->SetGPR(GPR::R1, OperandB);
    jit->Run(test_code.b32.size());

    REQUIRE(jit->GetGPR(GPR::R0) == u32(OperandA * OperandB));
  }
}