/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "basic_block.hpp"

namespace lunatic {
namespace frontend {

struct BasicBlockCache {
 ~BasicBlockCache() {
    /* Make sure that the cache does not consist of stale points,
     * once the basic blocks are deleted and X64Backend::OnBasicBlockToBeDeleted() will be called.
     */
    Flush();
  }

  void Flush() {
    for (int i = 0; i < 0x40000; i++) {
      data[i] = {};
    }
  }

  void Flush(u32 address_lo, u32 address_hi) {
    // bits  0 - 30: address[31:1]
    // bits 31 - 35: CPU mode
    // bit       36: thumb-flag
    address_lo >>= 1;
    address_hi >>= 1;

    // TODO: there probably is a faster way to implement this.
    for (u64 status = 0; status <= 63; status++) {
      for (u32 address = address_lo; address <= address_hi; address++) {
        auto key = BasicBlock::Key{(status << 31) | address};
        if (Get(key)) {
          Set(key, nullptr);
        }
      }
    }
  }

  auto Get(BasicBlock::Key key) const -> BasicBlock* {
    auto& table = data[key.value >> 19];
    if (table == nullptr) {
      return nullptr;
    }
    return table->data[key.value & 0x7FFFF].get();
  }

  void Set(BasicBlock::Key key, BasicBlock* block) {
    auto hash0 = key.value >> 19;
    auto hash1 = key.value & 0x7FFFF;
    auto& table = data[hash0];

    if (table == nullptr) {
      data[hash0] = std::make_unique<Table>();
    }

    auto current_block = std::move(table->data[hash1]);

    // Temporary fix: remove any linked blocks from the cache as well.
    if (current_block && current_block.get() != block) {
      for (auto linking_block : current_block->linking_blocks) {
        if (linking_block != current_block.get()) {
          Set(linking_block->key, nullptr);
        }
      }
    }

    table->data[hash1] = std::unique_ptr<BasicBlock>{block};
  }

  struct Table {
    // int use_count = 0;
    std::unique_ptr<BasicBlock> data[0x80000];
  };

  // TODO: better manage the lifetimes of the tables.
  std::unique_ptr<Table> data[0x40000];
};

} // namespace lunatic::frontend
} // namespace lunatic
