//
// Created by andre on 29/12/2025.
//

#pragma once
#include "../common/types.h"
#include <vector>
#include <stdexcept>
#include <cstddef>

class RAM {
    private:
        std::vector<cvsim::byte_t> data;

        void check(cvsim::addr_t addr, size_t bytes) const {
            if (addr > data.size() || bytes > data.size() - addr)
                throw std::out_of_range("[RAM] Address out of bounds!");
        }

    public:
        explicit RAM(size_t size) : data(size, 0) {}

        size_t size() const { return data.size(); }

        cvsim::byte_t read_byte(cvsim::addr_t addr) const {
            check(addr, 1);
            return data[addr];
        }

        cvsim::word_t read_word_aligned(cvsim::addr_t addr) const {
            if (addr & 0x3) throw std::runtime_error("[RAM] Unaligned word read!");
            check(addr, 4);
            return static_cast<cvsim::word_t>(data[addr]) |
                   (static_cast<cvsim::word_t>(data[addr + 1]) << 8) |
                   (static_cast<cvsim::word_t>(data[addr + 2]) << 16) |
                   (static_cast<cvsim::word_t>(data[addr + 3]) << 24);
        }

        void write_word_masked(cvsim::addr_t addr, cvsim::word_t value, uint8_t wstrb) {
            if (addr & 0x3) throw std::runtime_error("[RAM] Unaligned word write!");
            check(addr, 4);
            for (unsigned i = 0; i < 4; ++i)
                if (wstrb & (1u << i)) data[addr + i] = static_cast<cvsim::byte_t>(value >> (8 * i));
        }

        void load_binary(const std::vector<uint8_t>& program, cvsim::addr_t start_addr) {
            check(start_addr, program.size());
            for (size_t i = 0; i < program.size(); ++i) data[start_addr + i] = program[i];
        }
};
