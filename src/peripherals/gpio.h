#pragma once
#include "register_bank.h"

// GPIO: DATA (+0x00), DIRECTION (+0x04), INPUT (+0x08)
// The external input is read-only to software. Bits in DIRECTION set to 1 are outputs
class GPIO : public RegisterBank {
    cvsim::word_t inputs_ = 0;
public:
    GPIO() : RegisterBank(3) {}
    void set_inputs(cvsim::word_t value) { inputs_ = value; }
    cvsim::word_t outputs() const { return RegisterBank::read(0) & RegisterBank::read(4); }
    cvsim::word_t read(uint32_t offset) const override {
        return offset == 8 ? inputs_ : RegisterBank::read(offset);
    }
    void write(uint32_t offset, cvsim::word_t value, uint8_t lanes) override {
        if (offset == 8) return; // read-only input pins
        RegisterBank::write(offset,value,lanes);
    }
};
