#pragma once
#include "peripheral.h"
#include <stdexcept>

// DATA (+0x00), RESULT (+0x04), CONTROL (+0x08), CRC-32/ISO-HDLC.
class CRC32 : public Peripheral {
    uint32_t state_ = 0xffffffffu;

public:
    cvsim::word_t read(uint32_t offset) const override {
        if (offset == 4) return ~state_;
        if (offset == 0 || offset == 8) return 0;
        throw std::out_of_range("[CRC32] Invalid register");
    }

    void write(uint32_t offset, cvsim::word_t data, uint8_t lanes) override {
        if (offset == 4) return;
        if (offset == 8) {
            if ((lanes & 1) && (data & 1)) state_ = 0xffffffffu;
            return;
        }
        if (offset != 0) throw std::out_of_range("[CRC32] Invalid register");
        for (unsigned lane = 0; lane < 4; ++lane) {
            if (!(lanes & (1u << lane))) continue;
            state_ ^= (data >> (lane * 8)) & 0xffu;
            for (unsigned bit = 0; bit < 8; ++bit)
                state_ = (state_ >> 1) ^ ((state_ & 1u) ? 0xedb88320u : 0u);
        }
    }
};
