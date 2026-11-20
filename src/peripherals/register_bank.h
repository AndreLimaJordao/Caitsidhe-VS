#pragma once
#include "peripheral.h"
#include <cstddef>
#include <stdexcept>
#include <vector>

// Ready-to-use register peripheral
// Derived devices may override read/write for custom behavior
class RegisterBank : public Peripheral {
    std::vector<cvsim::word_t> registers_;
public:
    explicit RegisterBank(size_t count) : registers_(count, 0) {}
    cvsim::word_t read(uint32_t offset) const override {
        if ((offset & 3u) || offset / 4 >= registers_.size())
            throw std::out_of_range("[RegisterBank] Invalid register");
        return registers_[offset / 4];
    }
    void write(uint32_t offset, cvsim::word_t value, uint8_t lanes) override {
        if ((offset & 3u) || offset / 4 >= registers_.size())
            throw std::out_of_range("[RegisterBank] Invalid register");
        auto& reg = registers_[offset / 4];
        for (unsigned lane = 0; lane < 4; ++lane)
            if (lanes & (1u << lane)) {
                const auto mask = cvsim::word_t{0xff} << (lane * 8);
                reg = (reg & ~mask) | (value & mask);
            }
    }
};
