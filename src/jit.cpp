/*
 * Copyright (C) 2021 fleroviux
 */

#include <lunatic/cpu.hpp>
#include <vector>

#include "frontend/ir_opt/constant_propagation.hpp"
#include "frontend/ir_opt/context_load_store_elision.hpp"
#include "frontend/ir_opt/dead_code_elision.hpp"
#include "frontend/ir_opt/dead_flag_elision.hpp"
#include "frontend/state.hpp"
#include "frontend/translator/translator.hpp"

#include "backend/backend.hpp"

using namespace lunatic::frontend;
using namespace lunatic::backend;

namespace lunatic {

  class JIT final : public CPU {
    public:
      explicit JIT(CPU::Descriptor const& descriptor)
          : m_exception_base{descriptor.exception_base}
          , m_memory{descriptor.memory}
          , m_translator{descriptor} {
        m_backend = Backend::CreateBackend(descriptor, m_state, m_block_cache, m_irq_line);
        m_optimization_passes.push_back(std::make_unique<IRContextLoadStoreElisionPass>());
        m_optimization_passes.push_back(std::make_unique<IRDeadFlagElisionPass>());
        m_optimization_passes.push_back(std::make_unique<IRConstantPropagationPass>());
        m_optimization_passes.push_back(std::make_unique<IRDeadCodeElisionPass>());
      }

      void Reset() override {
        m_irq_line = false;
        m_wait_for_irq = false;
        m_cycles_to_run = 0;
        m_state.Reset();
        SetGPR(GPR::PC, m_exception_base);
        m_block_cache.Flush();
        m_exception_causing_basic_blocks.clear();
      }

      bool& IRQLine() override {
        return m_irq_line;
      }

      bool& WaitForIRQ() override {
        return m_wait_for_irq;
      }

      u32 GetExceptionBase() const override {
        return m_exception_base;
      }

      void SetExceptionBase(u32 new_exception_base) override {
        if (new_exception_base != m_exception_base) {
          // this is expected to happen rarely, so we just invalidate all blocks that may cause an exception.
          while (!m_exception_causing_basic_blocks.empty()) {
            m_block_cache.Set(m_exception_causing_basic_blocks.front()->key, nullptr);
          }

          m_translator.SetExceptionBase(new_exception_base);
          m_exception_base = new_exception_base;
        }
      }

      void ClearICache() override {
        m_block_cache.Flush();
      }

      void ClearICacheRange(u32 address_lo, u32 address_hi) override {
        m_block_cache.Flush(address_lo, address_hi);
      }

      auto Run(int cycles) -> int override {
        if (WaitForIRQ() && !IRQLine()) {
          return 0;
        }

        m_cycles_to_run += cycles;

        int cycles_available = m_cycles_to_run;

        while (m_cycles_to_run > 0) {
          if (IRQLine()) {
            SignalIRQ();
          }

          auto block_key = BasicBlock::Key{m_state};
          auto basic_block = m_block_cache.Get(block_key);
          auto hash = GetBasicBlockHash(block_key);

          if (basic_block == nullptr || basic_block->hash != hash) {
            basic_block = Compile(block_key);
          }

          m_cycles_to_run = m_backend->Call(*basic_block, m_cycles_to_run);

          if (WaitForIRQ()) {
            int cycles_executed = cycles_available - m_cycles_to_run;
            m_cycles_to_run = 0;
            return cycles_executed;
          }
        }

        return cycles_available - m_cycles_to_run;
      }

      u32 GetGPR(GPR reg) const override {
        return GetGPR(reg, GetCPSR().f.mode);
      }

      u32 GetGPR(GPR reg, Mode mode) const override {
        return const_cast<JIT*>(this)->GetGPR(reg, mode);
      }

      StatusRegister GetCPSR() const override {
        return const_cast<JIT*>(this)->GetCPSR();
      }

      StatusRegister GetSPSR(Mode mode) const override {
        return const_cast<JIT*>(this)->GetSPSR(mode);
      }

      void SetGPR(GPR reg, u32 value) override {
        SetGPR(reg, m_state.GetCPSR().f.mode, value);
      }

      void SetGPR(GPR reg, Mode mode, u32 value) override {
        m_state.GetGPR(mode, reg) = value;

        if (reg == GPR::PC) {
          if (GetCPSR().f.thumb) {
            m_state.GetGPR(mode, GPR::PC) += sizeof(u16) * 2;
          } else {
            m_state.GetGPR(mode, GPR::PC) += sizeof(u32) * 2;
          }
        }
      }

      void SetCPSR(StatusRegister value) override {
        m_state.GetCPSR() = value;
      }

      void SetSPSR(Mode mode, StatusRegister value) override {
        *m_state.GetPointerToSPSR(mode) = value;
      }

    private:
      BasicBlock* Compile(BasicBlock::Key block_key) {
        auto basic_block = new BasicBlock{block_key};

        basic_block->hash = GetBasicBlockHash(block_key);

        m_translator.Translate(*basic_block);
        Optimize(basic_block);

        if (basic_block->uses_exception_base) {
          m_exception_causing_basic_blocks.push_back(basic_block);

          basic_block->RegisterReleaseCallback([this](BasicBlock const& block) {
            auto match = std::find(
              m_exception_causing_basic_blocks.begin(), m_exception_causing_basic_blocks.end(), &block);

            if (match != m_exception_causing_basic_blocks.end()) {
              m_exception_causing_basic_blocks.erase(match);
            }
          });
        }

        m_backend->Compile(*basic_block);
        m_block_cache.Set(block_key, basic_block);
        basic_block->micro_blocks.clear();
        return basic_block;
      }

      void Optimize(BasicBlock* basic_block) {
        for (auto &micro_block : basic_block->micro_blocks) {
          for (auto& pass : m_optimization_passes) {
            pass->Run(micro_block.emitter);
          }
        }
      }

      void SignalIRQ() {
        auto& cpsr = GetCPSR();

        m_wait_for_irq = false;

        if (!cpsr.f.mask_irq) {
          GetSPSR(Mode::IRQ) = cpsr;

          cpsr.f.mode = Mode::IRQ;
          cpsr.f.mask_irq = 1;
          if (cpsr.f.thumb) {
            GetGPR(GPR::LR) = GetGPR(GPR::PC);
          } else {
            GetGPR(GPR::LR) = GetGPR(GPR::PC) - 4;
          }
          cpsr.f.thumb = 0;

          GetGPR(GPR::PC) = m_exception_base + 0x18 + sizeof(u32) * 2;
        }
      }

      u32 GetBasicBlockHash(BasicBlock::Key block_key) {
        return m_memory.FastRead<u32, Memory::Bus::Code>(block_key.Address());
      }

      u32& GetGPR(GPR reg) {
        return GetGPR(reg, GetCPSR().f.mode);
      }

      u32& GetGPR(GPR reg, Mode mode) {
        return m_state.GetGPR(mode, reg);
      }

      StatusRegister& GetCPSR() {
        return m_state.GetCPSR();
      }

      StatusRegister& GetSPSR(Mode mode) {
        return *m_state.GetPointerToSPSR(mode);
      }

      bool m_irq_line{false};
      bool m_wait_for_irq{false};
      int m_cycles_to_run{0};
      u32 m_exception_base;
      Memory& m_memory;
      State m_state{};
      Translator m_translator;
      BasicBlockCache m_block_cache{};
      std::unique_ptr<Backend> m_backend{};
      std::vector<std::unique_ptr<IRPass>> m_optimization_passes{};
      std::vector<BasicBlock*> m_exception_causing_basic_blocks{};
  };

  std::unique_ptr<CPU> CreateCPU(CPU::Descriptor const& descriptor) {
    return std::make_unique<JIT>(descriptor);
  }

} // namespace lunatic
