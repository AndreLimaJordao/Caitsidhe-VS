#pragma once
#include "peripheral.h"
#include <stdexcept>
#include <string>

// Minimal UART TX console: DATA (+0x00) sends low byte on a write to lane 0
// STATUS (+0x04): bit 0 TX ready (always 1), no RX/baud/interrupt modeling
class UART : public Peripheral {
    std::string output_;
public:
    cvsim::word_t read(uint32_t offset) const override {
        if (offset == 0) return 0;
        if (offset == 4) return 1;
        throw std::out_of_range("[UART] Invalid register");
    }
    void write(uint32_t offset, cvsim::word_t data, uint8_t wstrb) override {
        if (offset == 0) { if (wstrb & 1) output_ += char(data & 0xff); return; }
        if (offset == 4) return;
        throw std::out_of_range("[UART] Invalid register");
    }
    const std::string& output() const { return output_; }
    void clear() { output_.clear(); }
};
