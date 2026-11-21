#include "../src/core/cpu.h"
#include "check.h"
#include <iostream>
#include <string_view>
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
static void run(std::initializer_list<uint32_t> program, void (*check)(const CPU&, const Bus&),
                size_t ram_size = cvsim::MEM_SIZE) {
    RAM ram(ram_size);
    ram.load_binary(binary(program), 0);
    Bus bus(ram);
    CPU cpu(bus);
    for (int i = 0; i < 200 && !cpu.is_halted(); ++i) cpu.tick();
    CHECK(cpu.is_halted());
    check(cpu, bus);
}
static void run_fault(CPU& cpu, std::string_view message) {
    bool caught = false;
    try {
        for (int i = 0; i < 40 && !cpu.is_halted(); ++i) cpu.tick();
    } catch (const std::runtime_error& error) {
        CHECK(error.what() == message);
        caught = true;
    }
    CHECK(caught);
}

int main() {
    // ALU forwarding, signed immediates, arithmetic shift and x0.
    run({I(7,0,0,1), I(3,1,0,2), R(0x20,1,2,0,3), I(-1,0,0,4),
         I(1,4,5,5) | (1u << 30), I(12,0,0,0), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            CHECK(cpu.read_register(1) == 7);
            CHECK(cpu.read_register(2) == 10);
            CHECK(cpu.read_register(3) == 3);
            CHECK(cpu.read_register(5) == 0xFFFFFFFFu);
            CHECK(cpu.read_register(0) == 0);
        });
    // Store byte/halfword/word, signed/unsigned loads and load-use interlock.
    run({0x800000B7, I(-1,0,0,2), S(128,2,1,0), I(128,1,0,3,0x03),
         I(1,3,0,5), I(128,1,4,4,0x03), S(130,2,1,1), I(130,1,5,6,0x03),
         S(132,2,1,2), I(132,1,2,7,0x03), 0x00100073},
        [](const CPU& cpu, const Bus& bus) {
            CHECK(cpu.read_register(3) == 0xFFFFFFFFu);
            CHECK(cpu.read_register(4) == 255);
            CHECK(cpu.read_register(5) == 0);
            CHECK(cpu.read_register(6) == 65535);
            CHECK(cpu.read_register(7) == 0xFFFFFFFFu);
            CHECK(bus.read(cvsim::RESET_VECTOR + 132) == 0xFFFFFFFFu);
            CHECK(cpu.get_stats().load_use_stalls >= 1);
        });
    // Taken branch flushes wrong-path instruction; JAL jumps and writes return address.
    run({I(1,0,0,1), B(8,1,1,0), I(99,0,0,2), J(8,3), I(77,0,0,2),
         I(11,0,0,4), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            CHECK(cpu.read_register(2) == 0);
            CHECK(cpu.read_register(3) == cvsim::RESET_VECTOR + 16);
            CHECK(cpu.read_register(4) == 11);
            CHECK(cpu.get_stats().control_flushes >= 2);
        });
    // LUI / AUIPC / JALR, including immediate and cleared bit 0.
    run({0x123450B7, 0x00000117, I(17,2,0,3), I(3,3,0,4,0x67),
         I(99,0,0,5), I(88,0,0,5), I(5,0,0,6), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            CHECK(cpu.read_register(1) == 0x12345000u);
            CHECK(cpu.read_register(2) == cvsim::RESET_VECTOR + 4);
            CHECK(cpu.read_register(4) == cvsim::RESET_VECTOR + 16);
            CHECK(cpu.read_register(5) == 0);
            CHECK(cpu.read_register(6) == 5);
        });
    // Negative offset backward branch through a small counter loop.
    run({I(3,0,0,1), I(-1,1,0,1), B(-4,0,1,1), 0x00100073},
        [](const CPU& cpu, const Bus&) {
            CHECK(cpu.read_register(1) == 0);
            CHECK(cpu.get_stats().retired == 8);
        });
    // The last word jumps back into RAM. The fetch past it is on the wrong path.
    run({J(12,1), 0x00100073, I(0,0,0,0), J(-8,2)},
        [](const CPU& cpu, const Bus&) {
            CHECK(cpu.read_register(1) == cvsim::RESET_VECTOR + 4);
            CHECK(cpu.read_register(2) == cvsim::RESET_VECTOR + 16);
            CHECK(cpu.get_stats().retired == 3);
        }, 16);
    // An illegal instruction in ID can't cancel an earlier store or writeback.
    {
        RAM ram(64);
        Bus bus(ram);
        CPU cpu(bus);
        ram.load_binary(binary({0x800000B7, I(42,0,0,2), S(32,2,1,2),
                                I(7,0,0,3), 0, S(36,2,1,2)}), 0);
        run_fault(cpu, "[CPU] Illegal/unsupported instruction!");
        CHECK(bus.read(cvsim::RESET_VECTOR + 32) == 42);
        CHECK(bus.read(cvsim::RESET_VECTOR + 36) == 0);
        CHECK(cpu.read_register(3) == 7);
        CHECK(cpu.get_stats().retired == 4);
    }
    // A fault in MEM drains WB and discards younger instructions.
    {
        RAM ram(64);
        Bus bus(ram);
        CPU cpu(bus);
        ram.load_binary(binary({0x800000B7, I(42,0,0,2), I(1,1,2,3,0x03),
                                S(32,2,1,2), 0x00100073}), 0);
        run_fault(cpu, "[CPU] Misaligned data access!");
        CHECK(cpu.read_register(2) == 42);
        CHECK(bus.read(cvsim::RESET_VECTOR + 32) == 0);
        CHECK(cpu.get_stats().retired == 2);
    }
    // Byte-sized RAM, little-endian behavior, bounds and strobe masking.
    {
        RAM ram(8);
        ram.write_word_masked(0, 0x12345678u, 0xF);
        ram.write_word_masked(0, 0x0000AB00u, 0x2);
        CHECK(ram.read_word_aligned(0) == 0x1234AB78u);
        bool caught = false;
        try { ram.read_word_aligned(8); } catch (const std::out_of_range&) { caught = true; }
        CHECK(caught);
    }
    std::cout << "All Caitsidhe-VS smoke tests passed!\n";
}
