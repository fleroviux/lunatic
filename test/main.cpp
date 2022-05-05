/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <lunatic/cpu.hpp>
#include <fmt/format.h>
#include <fstream>
#include <cstring>
#include <SDL.h>
#include <unordered_map>

#ifdef _WIN32
  // Include required dependencies for SDL2
  #pragma comment(lib, "Imm32.lib")
  #pragma comment(lib, "Setupapi.lib")
  #pragma comment(lib, "Version.lib")
  #pragma comment(lib, "Winmm.lib")
#endif

/// NDS file header
struct Header {
  // TODO: add remaining header fields.
  // This should be enough for basic direct boot for now though.
  // http://problemkaputt.de/gbatek.htm#dscartridgeheader
  u8 game_title[12];
  u8 game_code[4];
  u8 maker_code[2];
  u8 unit_code;
  u8 encryption_seed_select;
  u8 device_capacity;
  u8 reserved0[8];
  u8 nds_region;
  u8 version;
  u8 autostart;
  struct Binary {
    u32 file_address;
    u32 entrypoint;
    u32 load_address;
    u32 size;
  } arm9, arm7;
};

struct Memory final : lunatic::Memory {
  Memory() {
    static constexpr u32 kDTCMBase  = 0x00800000;
    static constexpr u32 kDTCMLimit = 0x00803FFF;

    std::memset(dtcm, 0, sizeof(dtcm));
    std::memset(mainram, 0, sizeof(mainram));
    std::memset(vram, 0, sizeof(vram));
    vblank_flag = 0;
    vblank_counter = 0;

    pagetable = std::make_unique<std::array<u8*, 1048576>>();

    for (u64 address = 0; address < 0x100000000; address += 4096) {
      auto page = address >> 12;
      auto& entry = (*pagetable)[page];

      entry = nullptr;

      if (address >= kDTCMBase && address <= kDTCMLimit) {
        entry = &dtcm[(address - kDTCMBase) & 0x3FFF];
        continue;
      }

      switch (address >> 24) {
        case 0x02:
          entry = &mainram[address & 0x3FFFFF];
          break;
        case 0x06:
          entry = &vram[address & 0x1FFFFF];
          break;
      }
    }
  }

  auto ReadByte(u32 address, Bus bus) -> u8 override {
    if (address == 0x0400'0004) {
      if (++vblank_counter == 1000000) {
        vblank_counter = 0;
        vblank_flag ^= 1;
      }
      return vblank_flag;
    }

    fmt::print("unknown byte read @ 0x{:08X}\n", address);
    return 0;
  }

  auto ReadHalf(u32 address, Bus bus) -> u16 override {
    if (address == 0x0400'0004) {
      if (++vblank_counter == 1000000) {
        vblank_counter = 0;
        vblank_flag ^= 1;
      }
      return vblank_flag;
    }

    if (address == 0x0400'0130) {
      return keyinput;
    }

    fmt::print("unknown half read @ 0x{:08X}\n", address);
    return 0;
  }

  auto ReadWord(u32 address, Bus bus) -> u32 override {
    if (address == 0x0400'0004) {
      if (++vblank_counter == 1000000) {
        vblank_counter = 0;
        vblank_flag ^= 1;
      }
      return vblank_flag;
    }

    fmt::print("unknown word read @ 0x{:08X}\n", address);
    return 0;
  }

  void WriteByte(u32 address,  u8 value, Bus bus) override {
  }

  void WriteHalf(u32 address, u16 value, Bus bus) override {
  }

  void WriteWord(u32 address, u32 value, Bus bus) override {
  }

  int vblank_counter;
  u8 dtcm[0x4000];
  u8 mainram[0x400000];
  u8 vram[0x200000];
  int vblank_flag;
  u16 keyinput = 0x3FF;
};

struct StupidCP15 : lunatic::Coprocessor {
  auto Read(
    int opcode1,
    int cn,
    int cm,
    int opcode2
  ) -> u32 override {
    fmt::print("CP15 read: #{}, {}, {}, #{}\n", opcode1, cn, cm, opcode2);
    return 0xDEADBEEF;
  }

  void Write(
    int opcode1,
    int cn,
    int cm,
    int opcode2,
    u32 value
  ) override {
    fmt::print("CP15 write: #{}, {}, {}, #{} <- 0x{:08X}\n", opcode1, cn, cm, opcode2, value);
  }
};

static Memory g_memory;
static StupidCP15 g_cp15;
static u16 frame[256 * 192];

void render_frame() {
  for (int i = 0; i < 256 * 192; i++) {
    frame[i] = lunatic::read<u16>(&g_memory.vram, i * 2);
  }
}

int main(int argc, char** argv) {
  using namespace lunatic;

  static constexpr auto kROMPath = "armwrestler.nds";

  size_t size;
  std::ifstream file { kROMPath, std::ios::binary };

  if (!file.good()) {
    fmt::print("Failed to open {}.\n", kROMPath);
    return -1;
  }

  file.seekg(0, std::ios::end);
  size = file.tellg();
  file.seekg(0);

  auto header = Header{};
  file.read((char*)&header, sizeof(Header));
  if (!file.good()) {
    throw std::runtime_error("failed to read NDS ROM header");
  }

  // Load ARM9 binary into memory.
  {
    u8 data;
    u32 dst = header.arm9.load_address;
    file.seekg(header.arm9.file_address);
    for (u32 i = 0; i < header.arm9.size; i++) {
      file.read((char*)&data, 1);
      if (!file.good()) {
        throw std::runtime_error("failed to read ARM9 binary from ROM into ARM9 memory");
      }
      g_memory.FastWrite<u8, lunatic::Memory::Bus::Data>(dst++, data);
    }
  }

  // TODO: better initialization, set exception base for example.
  auto jit = CreateCPU(CPU::Descriptor{
    g_memory, {
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, &g_cp15
    }
  });
  jit->SetGPR(GPR::PC, header.arm9.entrypoint);

  SDL_Init(SDL_INIT_VIDEO);

  auto window = SDL_CreateWindow(
    "Project: lunatic",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    512,
    384,
    SDL_WINDOW_ALLOW_HIGHDPI);

  auto renderer = SDL_CreateRenderer(window, -1, 0);
  auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, 256, 192);

  auto event = SDL_Event{};

  auto tick0 = SDL_GetTicks();
  auto frames = 0;

  for (;;) {
    jit->Run(279620/3);
    render_frame();
    frames++;

    auto tick1 = SDL_GetTicks();
    auto diff = tick1 - tick0;

    if (diff >= 1000) {
      fmt::print("{} fps\n", frames * 1000.0/diff);
      tick0 = SDL_GetTicks();
      frames = 0;
    }

    SDL_UpdateTexture(texture, nullptr, frame, sizeof(u16) * 256);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        goto done;

      if (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) {
        auto bit  = -1;
        bool down = event.type == SDL_KEYDOWN;

        switch (reinterpret_cast<SDL_KeyboardEvent*>(&event)->keysym.sym) {
          case SDLK_a: bit = 0; break;
          case SDLK_s: bit = 1; break;
          case SDLK_BACKSPACE: bit = 2; break;
          case SDLK_RETURN: bit = 3; break;
          case SDLK_RIGHT: bit = 4; break;
          case SDLK_LEFT: bit = 5; break;
          case SDLK_UP: bit = 6; break;
          case SDLK_DOWN: bit = 7; break;
          case SDLK_f: bit = 8; break;
          case SDLK_d: bit = 9; break;
        }

        if (bit != -1) {
          if (down) {
            g_memory.keyinput &= ~(1 << bit);
          } else {
            g_memory.keyinput |=  (1 << bit);
          }
        }
      }
    }
  }

done:
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return 0;
}
