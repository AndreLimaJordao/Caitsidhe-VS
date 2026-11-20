#pragma once
#include "../common/types.h"
#include <cstdint>

// User-facing MMIO device interface. Addresses are offsets relative to the
// mapped device base, and accesses are always aligned 32-bit transactions
// wstrb selects active byte lanes; data is already shifted into those lanes
// Device lifetime must exceed the lifetime of any Bus referencing it
class Peripheral {
public:
    virtual ~Peripheral() = default;
    // Optional simulation cycle hook; no-op for purely combinational/register devices.
    virtual void tick() {}
    virtual cvsim::word_t read(uint32_t offset) const = 0;
    virtual void write(uint32_t offset, cvsim::word_t data, uint8_t wstrb) = 0;
};
