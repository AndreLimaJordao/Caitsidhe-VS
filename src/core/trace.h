#pragma once
#include "machine.h"
#include "pipeline_regs.h"
#include <ostream>

namespace cvsim {
    // JSON Lines, one event per line. The cycle starts at one.
    class Trace {
        std::ostream& out_;

        void access(const MemoryAccess& a) {
            if (!a.width) {
                out_ << "null";
                return;
            }
            out_ << "{\"address\":" << a.address << ",\"width\":" << a.width
                 << ",\"write\":" << (a.write ? "true" : "false") << ",\"value\":" << a.value << '}';
        }

    public:
        explicit Trace(std::ostream& out) : out_(out) {}

        void retire(uint64_t cycle, const MEM_WB_Reg& in) {
            out_ << "{\"event\":\"retire\",\"cycle\":" << cycle << ",\"pc\":" << in.pc
                 << ",\"instruction\":" << in.instruction << ",\"rd\":";
            if (in.regWrite && in.rd) out_ << unsigned(in.rd) << ",\"value\":" << in.result;
            else out_ << "null";
            out_ << ",\"memory\":";
            access(in.access);
            out_ << "}\n";
        }

        void memory(uint64_t cycle, addr_t pc, const MemoryAccess& a) {
            out_ << "{\"event\":\"memory\",\"cycle\":" << cycle << ",\"pc\":" << pc << ",\"access\":";
            access(a);
            out_ << "}\n";
        }

        void trap(uint64_t cycle, const Trap& trap, addr_t target) {
            out_ << "{\"event\":\"trap\",\"cycle\":" << cycle << ",\"pc\":" << trap.pc
                 << ",\"cause\":" << trap.cause << ",\"value\":" << trap.value
                 << ",\"target\":" << target << "}\n";
        }
    };
}
