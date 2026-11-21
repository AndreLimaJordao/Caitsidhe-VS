#pragma once
#include "ram.h"
#include "../peripherals/peripheral.h"
#include "../common/types.h"
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace MemoryMap {
    constexpr cvsim::addr_t RAM_BASE = cvsim::RESET_VECTOR;
}

class Bus {
    struct Region { uint64_t base, end; Peripheral* device; };
    RAM& ram;
    std::vector<Region> devices;

    static bool overlaps(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
        return a < d && c < b;
    }
    const Region* device_for(uint64_t address, unsigned bytes) const {
        for (const auto& region : devices)
            if (address >= region.base && address + bytes <= region.end) return &region;
        return nullptr;
    }
    bool is_ram(uint64_t address, unsigned bytes) const {
        return address >= MemoryMap::RAM_BASE &&
               address + bytes <= uint64_t(MemoryMap::RAM_BASE) + ram.size();
    }
    void check(uint64_t address, unsigned bytes) const {
        if (bytes == 0 || address + bytes > (uint64_t{1} << 32) ||
            (!is_ram(address, bytes) && !device_for(address, bytes)))
            throw std::out_of_range("[Bus] Address not mapped!");
    }
public:
    explicit Bus(RAM& ram_instance) : ram(ram_instance) {
        if (uint64_t(MemoryMap::RAM_BASE) + ram.size() > (uint64_t{1} << 32))
            throw std::invalid_argument("[Bus] RAM exceeds 32-bit address space");
    }
    void attach(cvsim::addr_t base, uint32_t size, Peripheral& peripheral) {
        const uint64_t end = uint64_t(base) + size;
        if (!size || (base & 3u) || (size & 3u) || end > (uint64_t{1} << 32))
            throw std::invalid_argument("[Bus] Device region must be aligned and nonempty");
        if (overlaps(base, end, MemoryMap::RAM_BASE, uint64_t(MemoryMap::RAM_BASE) + ram.size()))
            throw std::invalid_argument("[Bus] Device overlaps RAM");
        for (const auto& region : devices)
            if (overlaps(base, end, region.base, region.end))
                throw std::invalid_argument("[Bus] Device regions overlap");
        devices.push_back({base, end, &peripheral});
    }
    // Non-side-effecting address check
    void validate_write(cvsim::addr_t address, unsigned bytes) const {
        check(address, bytes);
    }
    cvsim::word_t fetch(cvsim::addr_t addr) const {
        if (!is_ram(addr, 4)) throw std::out_of_range("[Bus] Address not mapped!");
        return ram.read_word_aligned(addr - MemoryMap::RAM_BASE);
    }
    cvsim::word_t read(cvsim::addr_t addr) const {
        if (addr & 3u) throw std::runtime_error("[Bus] Unaligned word read");
        check(addr, 4);
        if (is_ram(addr, 4)) return ram.read_word_aligned(addr - MemoryMap::RAM_BASE);
        const auto* region = device_for(addr, 4);
        return region->device->read(static_cast<uint32_t>(uint64_t(addr) - region->base));
    }
    // One device read per CPU load, even for halfword and byte accesses
    cvsim::word_t read_access(cvsim::addr_t addr, unsigned bytes) const {
        if ((bytes != 1 && bytes != 2 && bytes != 4) || addr % bytes != 0 ||
            (addr & 3u) + bytes > 4)
            throw std::invalid_argument("[Bus] Invalid data access");
        check(addr, bytes);
        return (read(addr & ~3u) >> ((addr & 3u) * 8)) &
            (bytes == 4 ? 0xffffffffu : (uint32_t{1} << (8 * bytes)) - 1);
    }
    cvsim::byte_t read_byte(cvsim::addr_t addr) const {
        return static_cast<cvsim::byte_t>(read_access(addr, 1));
    }
    void write(cvsim::addr_t addr, cvsim::word_t data, uint8_t wstrb) {
        if (addr & 3u) throw std::runtime_error("[Bus] Unaligned word write");
        if (wstrb & 0xf0u) throw std::invalid_argument("[Bus] Invalid byte strobes");
        check(addr, 4);
        if (is_ram(addr, 4)) ram.write_word_masked(addr - MemoryMap::RAM_BASE, data, wstrb);
        else {
            const auto* region = device_for(addr, 4);
            region->device->write(static_cast<uint32_t>(uint64_t(addr) - region->base), data, wstrb);
        }
    }
};
