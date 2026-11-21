#pragma once
#include "bus.h"
#include <bit>

class Wishbone {
public:
    struct Signals {
        cvsim::addr_t adr = 0;
        cvsim::word_t dat = 0;
        uint8_t sel = 0;
        bool cyc = false;
        bool stb = false;
        bool we = false;
        bool ack = false;
        bool err = false;
        cvsim::word_t dat_r = 0;
    };

private:
    Bus& bus_;
    unsigned wait_states_;
    unsigned remaining_ = 0;
    Signals pins_{};

    void require_response() const {
        if (!pins_.cyc || remaining_ || pins_.ack || pins_.err)
            throw std::logic_error("[Wishbone] No transfer ready for a response");
    }

public:
    Wishbone(Bus& bus, unsigned wait_states) : bus_(bus), wait_states_(wait_states) {}

    bool request(cvsim::addr_t address, unsigned width, bool write, cvsim::word_t value) {
        if ((width != 1 && width != 2 && width != 4) || address % width != 0)
            throw std::invalid_argument("[Wishbone] Invalid transfer width or alignment");
        const unsigned lane = address & 3u;
        const auto sel = uint8_t(((1u << width) - 1u) << lane);
        const auto data = write ? value << (lane * 8) : 0u;
        const auto aligned = address & ~3u;
        if (!pins_.cyc) {
            pins_ = {aligned, data, sel, true, true, write, false, false};
            remaining_ = wait_states_;
        } else if (pins_.adr != aligned || pins_.dat != data || pins_.sel != sel || pins_.we != write) {
            throw std::logic_error("[Wishbone] Request changed before termination");
        }
        return remaining_ == 0;
    }

    cvsim::word_t read() {
        require_response();
        if (pins_.we) throw std::logic_error("[Wishbone] Expected a read request");
        try {
            const auto lane = std::countr_zero(pins_.sel);
            const auto data = bus_.read_access(pins_.adr + lane, std::popcount(pins_.sel));
            pins_.dat_r = data << (8 * lane);
            pins_.ack = true;
            return data;
        } catch (...) {
            pins_.err = true;
            throw;
        }
    }

    void write() {
        require_response();
        if (!pins_.we) throw std::logic_error("[Wishbone] Expected a write request");
        try {
            bus_.write(pins_.adr, pins_.dat, pins_.sel);
            pins_.ack = true;
        } catch (...) {
            pins_.err = true;
            throw;
        }
    }

    void clock() {
        if (pins_.ack || pins_.err) pins_.cyc = pins_.stb = pins_.ack = pins_.err = false;
        else if (remaining_) --remaining_;
    }

    void reset() { pins_ = {}; remaining_ = 0; }
    const Signals& signals() const { return pins_; }
};
