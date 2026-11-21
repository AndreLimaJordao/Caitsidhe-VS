#pragma once
#include "register_bank.h"

// COUNT (+0x00) read-only, COMPARE (+0x04), CONTROL (+0x08)
// CONTROL bit 0 enables counting; bit 1 resets the counter when written as 1
// When COMPARE is nonzero, count >= compare also raises the interrupt level.
class Timer : public RegisterBank {
    uint32_t count_ = 0;
public:
    Timer() : RegisterBank(3) {}
    cvsim::word_t read(uint32_t offset) const override {
        return offset == 0 ? count_ : RegisterBank::read(offset);
    }
    void write(uint32_t offset, cvsim::word_t value, uint8_t lanes) override {
        if (offset == 0) return;
        RegisterBank::write(offset,value,lanes);
        if (offset == 8 && (RegisterBank::read(8) & 2u)) {
            count_ = 0;
            RegisterBank::write(8, RegisterBank::read(8) & ~2u, 1); // clear reset bit, preserve enable
        }
    }
    void tick() override { if (RegisterBank::read(8) & 1u) ++count_; }
    bool elapsed() const { return RegisterBank::read(4) != 0 && count_ >= RegisterBank::read(4); }
    bool interrupt_pending() const override { return elapsed(); }
};
