#pragma once
#include <cstdint>
#include <stdexcept>

namespace cvsim {
    struct Trap {
        uint32_t cause;
        uint32_t value;
        uint32_t pc;
    };

    class TrapError : public std::runtime_error {
    public:
        const uint32_t cause;
        const uint32_t value;
        TrapError(const char* message, uint32_t why, uint32_t detail)
            : std::runtime_error(message), cause(why), value(detail) {}
    };

    // One hart, machine mode only. The board supplies the timer interrupt level.
    class Machine {
        static constexpr uint32_t MIE = 1u << 3;
        static constexpr uint32_t MPIE = 1u << 7;
        static constexpr uint32_t MPP = 3u << 11;
        static constexpr uint32_t MTIP = 1u << 7;
        uint32_t status_ = MPP, ie_ = 0, vector_ = 0, scratch_ = 0;
        uint32_t epc_ = 0, cause_ = 0, value_ = 0;
        bool timer_ = false;

    public:
        static constexpr uint32_t timer_cause = 0x80000007u;

        void set_timer(bool pending) { timer_ = pending; }
        bool timer_pending() const { return timer_ && (status_ & MIE) && (ie_ & MTIP); }

        uint32_t read(uint32_t csr) const {
            switch (csr) {
                case 0x300: return status_;
                case 0x301: return 0x40000100u; // MXL=32, I
                case 0x304: return ie_;
                case 0x305: return vector_;
                case 0x340: return scratch_;
                case 0x341: return epc_;
                case 0x342: return cause_;
                case 0x343: return value_;
                case 0x344: return timer_ ? MTIP : 0;
                case 0xf14: return 0; // mhartid
                default: throw std::runtime_error("[CPU] Unsupported CSR!");
            }
        }

        void write(uint32_t csr, uint32_t value) {
            switch (csr) {
                case 0x300: status_ = MPP | (value & (MIE | MPIE)); break;
                case 0x301: break; // Fixed WARL value
                case 0x304: ie_ = value & MTIP; break;
                case 0x305: vector_ = (value & ~3u) | ((value & 3u) == 1u ? 1u : 0u); break;
                case 0x340: scratch_ = value; break;
                case 0x341: epc_ = value & ~3u; break;
                case 0x342: cause_ = value; break;
                case 0x343: value_ = value; break;
                case 0x344: break; // MTIP is driven by the board
                default: throw std::runtime_error("[CPU] CSR is read-only or unsupported!");
            }
        }

        uint32_t enter(const Trap& trap) {
            epc_ = trap.pc & ~3u;
            cause_ = trap.cause;
            value_ = trap.value;
            status_ = MPP | ((status_ & MIE) ? MPIE : 0);
            const auto offset = (vector_ & 1u) && (cause_ >> 31) ? (cause_ & 0x7fffffffu) * 4 : 0;
            return (vector_ & ~3u) + offset;
        }

        uint32_t leave() {
            status_ = MPP | MPIE | ((status_ & MPIE) ? MIE : 0);
            return epc_;
        }
    };
}
