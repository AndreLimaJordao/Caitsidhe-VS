//
// Created by andre on 29/12/2025.
//

#pragma once
#include <cstdint>

namespace utils {
    // Extract field inst[msb:lsb] (including the full 32-bit width).
    constexpr uint32_t get_field(uint32_t value, int msb, int lsb) {
        const unsigned width = msb - lsb + 1;
        return (value >> lsb) & (width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1));
    }

    constexpr uint32_t get_bit(uint32_t value, int bit_pos) {
        return (value >> bit_pos) & 1u;
    }

    // Signed result in two's complement, avoiding signed-shift overflow.
    constexpr int32_t sign_extend(uint32_t value, int bits) {
        const uint32_t mask = bits == 32 ? 0xFFFFFFFFu : (1u << bits) - 1u;
        const uint32_t sign = 1u << (bits - 1);
        return static_cast<int32_t>(((value & mask) ^ sign) - sign);
    }

    constexpr uint32_t combine_bytes(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
        return (uint32_t(b3) << 24) | (uint32_t(b2) << 16) | (uint32_t(b1) << 8) | b0;
    }
}
