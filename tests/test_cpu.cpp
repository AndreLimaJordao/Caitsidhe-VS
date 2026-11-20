#include "../src/core/cpu.h"
#include <cassert>
#include <iostream>
#include <vector>

static uint32_t I(int imm, int rs1, int f3, int rd, int op = 0x13) {
    return (uint32_t(imm) & 0xFFFu) << 20 | uint32_t(rs1) << 15 | uint32_t(f3) << 12 | uint32_t(rd) << 7 | op;
}
static uint32_t R(int f7, int rs2, int rs1, int f3, int rd) {
    return uint32_t(f7) << 25 | uint32_t(rs2) << 20 | uint32_t(rs1) << 15 | uint32_t(f3) << 12 | uint32_t(rd) << 7 | 0x33;
}
static uint32_t S(int imm, int rs2, int rs1, int f3) {
    return (uint32_t(imm) & 0xFE0u) << 20 | uint32_t(rs2) << 20 | uint32_t(rs1) << 15 |
           uint32_t(f3) << 12 | (uint32_t(imm) & 31u) << 7 | 0x23;
}
static uint32_t B(int imm, int rs2, int rs1, int f3) {
    const uint32_t x = uint32_t(imm);
    return ((x >> 12) & 1) << 31 | ((x >> 5) & 63) << 25 | uint32_t(rs2) << 20 |
           uint32_t(rs1) << 15 | uint32_t(f3) << 12 | ((x >> 1) & 15) << 8 |
           ((x >> 11) & 1) << 7 | 0x63;
}
static uint32_t J(int imm, int rd) {
    const uint32_t x = uint32_t(imm);
    return ((x >> 20) & 1) << 31 | ((x >> 1) & 1023) << 21 |
           ((x >> 11) & 1) << 20 | ((x >> 12) & 255) << 12 | uint32_t(rd) << 7 | 0x6F;
}
static std::vector<uint8_t> binary(std::initializer_list<uint32_t> ins) {
    std::vector<uint8_t> bytes;
    for (auto x : ins) for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(x >> (i * 8)));
    return bytes;
}
static void run(std::initializer_list<uint32_t> program, void (*check)(const CPU&, const Bus&)) {
    RAM ram(cvsim::MEM_SIZE);
    ram.load_binary(binary(program), 0);
    Bus bus(ram);
    CPU cpu(bus);
    for (int i = 0; i < 200 && !cpu.is_halted(); ++i) cpu.tick();
    assert(cpu.is_halted());
    check(cpu, bus);
}
int main() {
    // ALU forwarding, signed immediates, arithmetic shift and x0.
    run({I(7,0,0,1), I(3,1,0,2), R(0x20,1,2,0,3), I(-1,0,0,4),
         I(1,4,5,5) | (1u << 30), I(12,0,0,0), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            assert(cpu.read_register(1) == 7);
            assert(cpu.read_register(2) == 10);
            assert(cpu.read_register(3) == 3);
            assert(cpu.read_register(5) == 0xFFFFFFFFu);
            assert(cpu.read_register(0) == 0);
        });
    // Store byte/halfword/word, signed/unsigned loads and load-use interlock.
    run({0x800000B7, I(-1,0,0,2), S(128,2,1,0), I(128,1,0,3,0x03),
         I(1,3,0,5), I(128,1,4,4,0x03), S(130,2,1,1), I(130,1,5,6,0x03),
         S(132,2,1,2), I(132,1,2,7,0x03), 0x00100073},
        [](const CPU& cpu, const Bus& bus) {
            assert(cpu.read_register(3) == 0xFFFFFFFFu);
            assert(cpu.read_register(4) == 255);
            assert(cpu.read_register(5) == 0);
            assert(cpu.read_register(6) == 65535);
            assert(cpu.read_register(7) == 0xFFFFFFFFu);
            assert(bus.read(cvsim::RESET_VECTOR + 132) == 0xFFFFFFFFu);
            assert(cpu.get_stats().load_use_stalls >= 1);
        });
    // Taken branch flushes wrong-path instruction; JAL jumps and writes return address.
    run({I(1,0,0,1), B(8,1,1,0), I(99,0,0,2), J(8,3), I(77,0,0,2),
         I(11,0,0,4), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            assert(cpu.read_register(2) == 0);
            assert(cpu.read_register(3) == cvsim::RESET_VECTOR + 16);
            assert(cpu.read_register(4) == 11);
            assert(cpu.get_stats().control_flushes >= 2);
        });
    // LUI / AUIPC / JALR, including immediate and cleared bit 0.
    run({0x123450B7, 0x00000117, I(17,2,0,3), I(3,3,0,4,0x67),
         I(99,0,0,5), I(88,0,0,5), I(5,0,0,6), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            assert(cpu.read_register(1) == 0x12345000u);
            assert(cpu.read_register(2) == cvsim::RESET_VECTOR + 4);
            assert(cpu.read_register(4) == cvsim::RESET_VECTOR + 16);
            assert(cpu.read_register(5) == 0);
            assert(cpu.read_register(6) == 5);
        });
    // Negative offset backward branch through a small counter loop.
    run({I(3,0,0,1), I(-1,1,0,1), B(-4,0,1,1), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            assert(cpu.read_register(1) == 0);
            assert(cpu.get_stats().retired == 8);
        });
    // Byte-sized RAM, little-endian behavior, bounds and strobe masking.
    {
        RAM ram(8);
        ram.write_word_masked(0, 0x12345678u, 0xF);
        ram.write_word_masked(0, 0x0000AB00u, 0x2);
        assert(ram.read_word_aligned(0) == 0x1234AB78u);
        bool caught = false;
        try { ram.read_word_aligned(8); } catch (const std::out_of_range&) { caught = true; }
        assert(caught);
    }
    std::cout << "All Caitsidhe-VS smoke tests passed!\n";
}
