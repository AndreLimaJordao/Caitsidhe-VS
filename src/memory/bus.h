//
// Created by andre on 29/12/2025.
//

#pragma once
#include "ram.h"
#include "../common/types.h"
#include <stdexcept>

namespace MemoryMap {
    constexpr cvsim::addr_t RAM_BASE = cvsim::RESET_VECTOR;
    // TODO: Add MMIO peripherals and Wishbone handshakes here.
}

class Bus {
    private:
        RAM& ram;

        cvsim::addr_t offset(cvsim::addr_t addr) const {
            if (addr < MemoryMap::RAM_BASE ||
                static_cast<uint64_t>(addr) - MemoryMap::RAM_BASE >= ram.size())
                throw std::out_of_range("[Bus] Address not mapped!");
            return addr - MemoryMap::RAM_BASE;
        }

    public:
        explicit Bus(RAM& ram_instance) : ram(ram_instance) {}

        cvsim::word_t read(cvsim::addr_t addr) const {
            return ram.read_word_aligned(offset(addr));
        }

        void write(cvsim::addr_t addr, cvsim::word_t data, uint8_t wstrb) {
            ram.write_word_masked(offset(addr), data, wstrb);
        }

        cvsim::byte_t read_byte(cvsim::addr_t addr) const {
            return ram.read_byte(offset(addr));
        }
};
