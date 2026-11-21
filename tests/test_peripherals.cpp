#include "../src/core/cpu.h"
#include "../src/peripherals/register_bank.h"
#include "check.h"
#include <iostream>
#include <stdexcept>
#include <vector>

static std::vector<uint8_t> bytes(std::initializer_list<uint32_t> instructions) {
    std::vector<uint8_t> out;
    for (auto x : instructions) for (int i=0; i<4; ++i) out.push_back(uint8_t(x >> (8*i)));
    return out;
}
class Counter : public Peripheral {
public:
    mutable int reads = 0;
    int writes = 0;
    uint32_t last = 0;
    uint8_t lanes = 0;
    uint32_t read(uint32_t offset) const override {
        CHECK(offset == 0);
        ++reads;
        return 0x12345678;
    }
    void write(uint32_t offset, uint32_t value, uint8_t wstrb) override {
        CHECK(offset == 0);
        ++writes; last = value; lanes = wstrb;
    }
};
int main() {
    RAM ram(1024);
    Bus bus(ram);
    Counter c;
    RegisterBank bank(4);
    bus.attach(0x90000000, 4, c);
    bus.attach(0x90001000, 16, bank);
    bool caught = false;
    try { bus.attach(0x90000000, 4, bank); } catch (const std::invalid_argument&) { caught = true; }
    CHECK(caught);
    caught = false;
    try { bus.attach(cvsim::RESET_VECTOR, 4, bank); } catch (const std::invalid_argument&) { caught = true; }
    CHECK(caught);
    CHECK(bus.read_access(0x90000001, 1) == 0x56);
    CHECK(bus.read_access(0x90000002, 2) == 0x1234);
    CHECK(c.reads == 2);
    bus.validate_write(0x90000002, 1);
    CHECK(c.reads == 2 && c.writes == 0);
    bus.write(0x90000000, 0x00ab0000, 0x4);
    CHECK(c.writes == 1 && c.last == 0x00ab0000 && c.lanes == 4);
    bus.write(0x90001000, 0xabcdef12, 0xf);
    bus.write(0x90001000, 0x00005500, 0x2);
    CHECK(bus.read(0x90001000) == 0xabcd5512);
    // Execute real RV32I MMIO load/store through the pipeline.
    // lui x1, 0x90000; addi x2,x0,42; sw x2,0(x1); lw x3,0(x1); ebreak
    ram.load_binary(bytes({0x900000b7, 0x02a00113, 0x0020a023, 0x0000a183, 0x00100073}), 0);
    CPU cpu(bus);
    for (int i=0; i<40 && !cpu.is_halted(); ++i) cpu.tick();
    CHECK(cpu.is_halted());
    CHECK(c.writes == 2 && c.last == 42 && c.lanes == 15);
    CHECK(cpu.read_register(3) == 0x12345678);
    CHECK(c.reads == 3); // no speculative read at store validation

    // A device may reject a write even if the address is mapped in the bus.
    RegisterBank small_bank(1);
    bus.attach(0x90002000, 8, small_bank);
    // lui x1, 0x90002; addi x2,x0,42; sw x2,4(x1); sw x2,0(x1); ebreak
    ram.load_binary(bytes({0x900020b7, 0x02a00113, 0x0020a223, 0x0020a023, 0x00100073}), 0);
    cpu.reset();
    caught = false;
    try {
        for (int i=0; i<40 && !cpu.is_halted(); ++i) cpu.tick();
    } catch (const std::out_of_range&) {
        caught = true;
    }
    CHECK(caught);
    CHECK(cpu.read_register(2) == 42);
    CHECK(cpu.get_stats().retired == 2);
    CHECK(small_bank.read(0) == 0);
    std::cout << "MMIO device and pipeline tests passed\n";
}
