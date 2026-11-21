#pragma once
#include "../core/cpu.h"
#include "../memory/bus.h"
#include "../memory/ram.h"
#include "../peripherals/peripheral.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>

class SoC {
    RAM memory_;
    Bus bus_;
    CPU cpu_;
    std::vector<Peripheral*> peripherals_;
    const Peripheral* timer_interrupt_ = nullptr;
public:
    explicit SoC(size_t ram_bytes = cvsim::MEM_SIZE, CPU::Options options = {}) : memory_(ram_bytes), bus_(memory_), cpu_(bus_, options) {}
    SoC(const SoC&) = delete;
    SoC& operator=(const SoC&) = delete;
    void attach(cvsim::addr_t base, uint32_t bytes, Peripheral& device) {
        bus_.attach(base, bytes, device);
        if (std::find(peripherals_.begin(), peripherals_.end(), &device) == peripherals_.end())
            peripherals_.push_back(&device);
    }
    void load(const std::vector<uint8_t>& binary, uint32_t offset = 0) {
        memory_.load_binary(binary, offset);
    }
    CPU& cpu() { return cpu_; }
    const CPU& cpu() const { return cpu_; }
    Bus& bus() { return bus_; }
    const Bus& bus() const { return bus_; }
    RAM& ram() { return memory_; }
    const RAM& ram() const { return memory_; }
    void set_timer_interrupt_source(const Peripheral& device) { timer_interrupt_ = &device; }
    void tick() {
        if (timer_interrupt_) cpu_.set_timer_interrupt(timer_interrupt_->interrupt_pending());
        cpu_.tick();
        for (auto* peripheral : peripherals_) peripheral->tick();
    }
};
