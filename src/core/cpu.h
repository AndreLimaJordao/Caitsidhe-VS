//
// Created by andre on 29/12/2025.
//

#pragma once
#include "../common/types.h"
#include "../common/utils.h"
#include "../memory/bus.h"
#include "../memory/wishbone.h"
#include "alu.h"
#include "genImm.h"
#include "pipeline_regs.h"
#include "regfile.h"
#include "machine.h"
#include "trace.h"
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <iostream>

class CPU {
    public:
        struct Stats {
            uint64_t cycles = 0;
            uint64_t retired = 0;
            uint64_t load_use_stalls = 0;
            uint64_t control_flushes = 0;
            uint64_t data_wait_stalls = 0;
            double cpi() const { return retired ? double(cycles) / retired : 0.0; }
        };
        struct Options {
            unsigned wait_states = 0;
            bool machine_mode = false;
        };

    private:
        RegFile regFile;
        ALU alu;
        Bus& bus;
        Wishbone data_bus;
        Options options;
        cvsim::Machine machine;
        cvsim::Trace* trace = nullptr;
        std::optional<cvsim::Trap> trap_next;
        cvsim::addr_t pc = cvsim::RESET_VECTOR;
        cvsim::addr_t pc_next = cvsim::RESET_VECTOR;

        cvsim::IF_ID_Reg IF_ID_Curr{}, IF_ID_Next{};
        cvsim::ID_EX_Reg ID_EX_Curr{}, ID_EX_Next{};
        cvsim::EX_MEM_Reg EX_MEM_Curr{}, EX_MEM_Next{};
        cvsim::MEM_WB_Reg MEM_WB_Curr{}, MEM_WB_Next{};
        Stats stats{};
        bool fetch_stopped = false;
        bool fetch_stopped_next = false;
        bool halted = false;
        bool evaluated = false;
        bool load_stall_next = false;
        bool control_flush_next = false;
        bool data_wait_next = false;

        static uint8_t opcode(cvsim::inst_t inst) { return inst & 0x7F; }
        static uint8_t funct3(cvsim::inst_t inst) { return (inst >> 12) & 7; }
        static uint8_t rs1(cvsim::inst_t inst) { return (inst >> 15) & 31; }
        static uint8_t rs2(cvsim::inst_t inst) { return (inst >> 20) & 31; }
        static uint8_t rd(cvsim::inst_t inst) { return (inst >> 7) & 31; }

        static bool uses_rs1(cvsim::inst_t inst) {
            switch (opcode(inst)) {
                case 0x03: 
                case 0x13: 
                case 0x23: 
                case 0x33: 
                case 0x63: 
                case 0x67: return true;
                case 0x73: return funct3(inst) >= 1 && funct3(inst) <= 3;
                default: return false;
            }
        }
        static bool uses_rs2(cvsim::inst_t inst) {
            switch (opcode(inst)) {
                case 0x23: 
                case 0x33: 
                case 0x63: return true;
                default: return false;
            }
        }
        static bool is_ebreak(cvsim::inst_t inst) { return inst == 0x00100073; }
        static bool is_csr(cvsim::inst_t inst) {
            return opcode(inst) == 0x73 && funct3(inst) != 0 && funct3(inst) != 4;
        }
        static constexpr cvsim::inst_t ECALL = 0x00000073;
        static constexpr cvsim::inst_t MRET = 0x30200073;

        static cvsim::Fault current_fault(uint32_t cause, uint32_t value) {
            const auto error = std::current_exception();
            try {
                std::rethrow_exception(error);
            } catch (const cvsim::TrapError& trap) {
                return {error, trap.cause, trap.value};
            } catch (...) {
                return {error, cause, value};
            }
        }

        template <typename Out, typename In, typename Stage>
        static Out evaluate_stage(const In& in, Stage stage, uint32_t cause, uint32_t value) {
            if (!in.valid) return {};
            Out out{};
            out.pc = in.pc;
            out.instruction = in.instruction;
            out.valid = true;
            out.fault = in.fault;
            if (!out.fault) {
                try {
                    return stage(in);
                } catch (...) {
                    out.fault = current_fault(cause, value);
                }
            }
            return out;
        }

        cvsim::IF_ID_Reg fetch(cvsim::addr_t address) const {
            try {
                if (address & 3u) throw cvsim::TrapError("[CPU] Instruction address misaligned!", 0, address);
                return {address, bus.fetch(address), true, {}};
            } catch (...) {
                // IF is speculative. An older redirect can still discard this fetch.
                return {address, 0, true, current_fault(1, address)};
            }
        }

        cvsim::ID_EX_Reg decode(const cvsim::IF_ID_Reg& in) const {
            cvsim::ID_EX_Reg out{};
            auto inst = in.instruction;
            out.pc = in.pc;
            out.instruction = inst;
            out.valid = true;
            out.opcode = static_cast<cvsim::Opcode>(opcode(inst));
            out.rd = rd(inst);
            out.rs1 = rs1(inst);
            out.rs2 = rs2(inst);
            out.uses_rs1 = uses_rs1(inst);
            out.uses_rs2 = uses_rs2(inst);
            out.rs1_val = regFile.read(out.rs1);
            out.rs2_val = regFile.read(out.rs2);
            // Writeback at this clock edge is visible to decode in this cycle.
            if (MEM_WB_Curr.valid && MEM_WB_Curr.regWrite && MEM_WB_Curr.rd != 0) {
                if (out.uses_rs1 && out.rs1 == MEM_WB_Curr.rd) out.rs1_val = MEM_WB_Curr.result;
                if (out.uses_rs2 && out.rs2 == MEM_WB_Curr.rd) out.rs2_val = MEM_WB_Curr.result;
            }
            switch (opcode(inst)) {
                case 0x33: out.instr_type = cvsim::InstrType::R_TYPE; out.regWrite = true; break;
                case 0x13: out.instr_type = cvsim::InstrType::I_TYPE; out.regWrite = true; break;
                case 0x03: out.instr_type = cvsim::InstrType::I_TYPE; out.regWrite = true; out.memRead = true; break;
                case 0x23: out.instr_type = cvsim::InstrType::S_TYPE; out.memWrite = true; break;
                case 0x63: out.instr_type = cvsim::InstrType::B_TYPE; break;
                case 0x37: case 0x17: out.instr_type = cvsim::InstrType::U_TYPE; out.regWrite = true; break;
                case 0x6F: out.instr_type = cvsim::InstrType::J_TYPE; out.regWrite = true; break;
                case 0x67: out.instr_type = cvsim::InstrType::I_TYPE; out.regWrite = true; break;
                case 0x0F: if (funct3(inst) == 0) break; [[fallthrough]];
                case 0x73:
                    if (is_ebreak(inst)) break;
                    if (options.machine_mode && opcode(inst) == 0x73) {
                        if (inst == ECALL || inst == MRET) break;
                        if (is_csr(inst)) { out.regWrite = true; break; }
                    }
                    [[fallthrough]];
                default: throw std::runtime_error("[CPU] Illegal/unsupported instruction!");
            }
            out.imm = cvsim::genImm(inst, out.instr_type);
            return out;
        }

        cvsim::word_t forward(cvsim::reg_addr_t reg, cvsim::word_t value) const {
            if (reg == 0) return 0;
            if (EX_MEM_Curr.valid && EX_MEM_Curr.regWrite && !EX_MEM_Curr.memRead && EX_MEM_Curr.rd == reg)
                return EX_MEM_Curr.alu_result;
            if (MEM_WB_Curr.valid && MEM_WB_Curr.regWrite && MEM_WB_Curr.rd == reg)
                return MEM_WB_Curr.result;
            return value;
        }

        cvsim::EX_MEM_Reg execute(const cvsim::ID_EX_Reg& in, bool& redirect, cvsim::addr_t& target) {
            cvsim::EX_MEM_Reg out{};
            const auto inst = in.instruction;
            const auto op = opcode(inst);
            const auto f3 = funct3(inst);
            const auto f7 = (inst >> 25) & 0x7F;
            const auto lhs = in.uses_rs1 ? forward(in.rs1, in.rs1_val) : 0;
            const auto rhs = in.uses_rs2 ? forward(in.rs2, in.rs2_val) : 0;
            cvsim::word_t result = 0;
            out.pc = in.pc;
            out.instruction = inst;
            out.rd = in.rd;
            out.funct3 = f3;
            out.regWrite = in.regWrite;
            out.memRead = in.memRead;
            out.memWrite = in.memWrite;
            out.rs2_val = rhs;
            out.valid = true;
            if (op == 0x33 || op == 0x13) {
                const auto arg = op == 0x33 ? rhs : in.imm;
                cvsim::ALUOp operation;
                switch (f3) {
                    case 0: operation = (op == 0x33 && f7 == 0x20) ? cvsim::ALUOp::SUB : cvsim::ALUOp::ADD; break;
                    case 1: operation = cvsim::ALUOp::SLL; break;
                    case 2: operation = cvsim::ALUOp::SLT; break;
                    case 3: operation = cvsim::ALUOp::SLTU; break;
                    case 4: operation = cvsim::ALUOp::XOR; break;
                    case 5: operation = (op == 0x33 ? f7 == 0x20 : (inst >> 30) & 1) ? cvsim::ALUOp::SRA : cvsim::ALUOp::SRL; break;
                    case 6: operation = cvsim::ALUOp::OR; break;
                    case 7: operation = cvsim::ALUOp::AND; break;
                    default: throw std::runtime_error("[CPU] Invalid ALU operation!");
                }
                if ((op == 0x33 && f7 != 0 && !(f7 == 0x20 && (f3 == 0 || f3 == 5))) ||
                    (op == 0x13 && ((f3 == 1 && f7 != 0) || (f3 == 5 && f7 != 0 && f7 != 0x20))))
                    throw std::runtime_error("[CPU] Unsupported ALU encoding!");
                result = alu.execute(operation, lhs, arg).value;
            } else if (op == 0x03 || op == 0x23) {
                result = lhs + in.imm;
                if ((f3 != 0 && f3 != 1 && f3 != 2 && !(op == 0x03 && (f3 == 4 || f3 == 5))))
                    throw std::runtime_error("[CPU] Unsupported memory access!");
            } else if (op == 0x37) result = in.imm;
            else if (op == 0x17) result = in.pc + in.imm;
            else if (op == 0x6F || op == 0x67) {
                if (op == 0x67 && f3 != 0) throw std::runtime_error("[CPU] Invalid JALR encoding!");
                redirect = true;
                target = op == 0x6F ? in.pc + in.imm : (lhs + in.imm) & ~1u;
                result = in.pc + 4;
            } else if (op == 0x63) {
                bool take = false;
                switch (f3) {
                    case 0: take = lhs == rhs; break;
                    case 1: take = lhs != rhs; break;
                    case 4: take = static_cast<int32_t>(lhs) < static_cast<int32_t>(rhs); break;
                    case 5: take = static_cast<int32_t>(lhs) >= static_cast<int32_t>(rhs); break;
                    case 6: take = lhs < rhs; break;
                    case 7: take = lhs >= rhs; break;
                    default: throw std::runtime_error("[CPU] Invalid branch encoding!");
                }
                if (take) { redirect = true; target = in.pc + in.imm; }
            } else if (op == 0x0F) {
                // The in-order data port completes older accesses before FENCE moves on.
            } else if (is_csr(inst)) {
                const auto csr = inst >> 20;
                result = machine.read(csr);
                const auto source = (f3 & 4u) ? uint32_t(in.rs1) : lhs;
                const auto operation = f3 & 3u;
                if (operation == 1 || in.rs1 != 0) {
                    if ((csr & 0xc00u) == 0xc00u) throw std::runtime_error("[CPU] CSR is read-only!");
                    if (operation == 1) out.csr_write = source;
                    else if (operation == 2) out.csr_write = result | source;
                    else out.csr_write = result & ~source;
                }
            } else if (op == 0x73) {
                if (inst == ECALL) throw cvsim::TrapError("[CPU] ECALL", 11, 0);
                if (options.machine_mode && is_ebreak(inst))
                    throw cvsim::TrapError("[CPU] Breakpoint!", 3, in.pc);
            } else throw std::runtime_error("[CPU] Unsupported opcode!");
            if (redirect && (target & 3u))
                throw cvsim::TrapError("[CPU] Misaligned branch target!", 0, target);
            out.alu_result = result;
            return out;
        }

        cvsim::MEM_WB_Reg memory(const cvsim::EX_MEM_Reg& in) {
            cvsim::MEM_WB_Reg out{};
            out.pc = in.pc;
            out.instruction = in.instruction;
            out.rd = in.rd;
            out.regWrite = in.regWrite;
            out.valid = true;
            out.result = in.alu_result;
            out.csr_write = in.csr_write;
            if (in.memRead || in.memWrite) {
                const unsigned width = in.funct3 == 0 || in.funct3 == 4 ? 1 : in.funct3 == 1 || in.funct3 == 5 ? 2 : 4;
                const auto addr = in.alu_result;
                if (addr % width != 0)
                    throw cvsim::TrapError("[CPU] Misaligned data access!", in.memWrite ? 6 : 4, addr);
                const auto mask = width == 4 ? 0xffffffffu : (1u << (width * 8)) - 1;
                const auto data = in.rs2_val & mask;
                if (!data_bus.request(addr, width, in.memWrite, data)) {
                    data_wait_next = true;
                    return {};
                }
                out.access = {addr, data, width, in.memWrite};
                if (in.memRead) {
                    const auto raw = data_bus.read();
                    out.access.value = raw;
                    if (in.funct3 == 0) out.result = utils::sign_extend(raw, 8);
                    else if (in.funct3 == 1) out.result = utils::sign_extend(raw, 16);
                    else out.result = raw;
                }
            }
            return out;
        }

        cvsim::addr_t resume_pc() const {
            if (EX_MEM_Curr.valid) return EX_MEM_Curr.pc;
            if (ID_EX_Curr.valid) return ID_EX_Curr.pc;
            if (IF_ID_Curr.valid) return IF_ID_Curr.pc;
            return pc;
        }

    public:
        explicit CPU(Bus& system_bus) : CPU(system_bus, {}) {}
        CPU(Bus& system_bus, Options config)
            : bus(system_bus), data_bus(system_bus, config.wait_states), options(config) { reset(); }

        void reset() {
            regFile.reset();
            data_bus.reset();
            machine = {};
            trap_next.reset();
            pc = pc_next = cvsim::RESET_VECTOR;
            IF_ID_Curr = IF_ID_Next = {};
            ID_EX_Curr = ID_EX_Next = {};
            EX_MEM_Curr = EX_MEM_Next = {};
            MEM_WB_Curr = MEM_WB_Next = {};
            stats = {};
            fetch_stopped = fetch_stopped_next = halted = evaluated = false;
            load_stall_next = control_flush_next = data_wait_next = false;
        }

        void eval() {
            if (halted) return;
            if (evaluated) throw std::logic_error("[CPU] eval() already called!");
            // Report the fault after every older instruction had its chance to commit.
            if (MEM_WB_Curr.fault && !options.machine_mode)
                std::rethrow_exception(MEM_WB_Curr.fault.error);
            evaluated = true;
            load_stall_next = control_flush_next = data_wait_next = false;
            trap_next.reset();
            pc_next = pc;
            fetch_stopped_next = fetch_stopped;
            IF_ID_Next = {};
            ID_EX_Next = {};
            EX_MEM_Next = {};
            MEM_WB_Next = {};
            if (MEM_WB_Curr.fault) {
                trap_next = cvsim::Trap{MEM_WB_Curr.fault.cause, MEM_WB_Curr.fault.value, MEM_WB_Curr.pc};
                return;
            }
            // Let an in-flight data beat finish, and let CSR changes reach WB.
            const bool system_in_wb = MEM_WB_Curr.valid && opcode(MEM_WB_Curr.instruction) == 0x73;
            if (options.machine_mode && machine.timer_pending() && !system_in_wb &&
                !data_bus.signals().cyc && !EX_MEM_Curr.fault && !ID_EX_Curr.fault && !IF_ID_Curr.fault) {
                trap_next = cvsim::Trap{cvsim::Machine::timer_cause, 0, resume_pc()};
                return;
            }
            MEM_WB_Next = evaluate_stage<cvsim::MEM_WB_Reg>(EX_MEM_Curr,
                [this](const auto& in) { return memory(in); },
                EX_MEM_Curr.memWrite ? 7 : 5, EX_MEM_Curr.alu_result);
            if (data_wait_next) return;
            if (MEM_WB_Next.fault) {
                fetch_stopped_next = true;
                return;
            }

            bool redirect = false;
            cvsim::addr_t target = 0;
            EX_MEM_Next = evaluate_stage<cvsim::EX_MEM_Reg>(ID_EX_Curr,
                [this, &redirect, &target](const auto& in) { return execute(in, redirect, target); },
                2, ID_EX_Curr.instruction);
            if (EX_MEM_Next.fault) {
                fetch_stopped_next = true;
                return;
            }

            const bool stall = ID_EX_Curr.valid && ID_EX_Curr.memRead && ID_EX_Curr.rd != 0 && IF_ID_Curr.valid && ((uses_rs1(IF_ID_Curr.instruction) && rs1(IF_ID_Curr.instruction) == ID_EX_Curr.rd) || (uses_rs2(IF_ID_Curr.instruction) && rs2(IF_ID_Curr.instruction) == ID_EX_Curr.rd));
            if (redirect) {
                pc_next = target;
                fetch_stopped_next = false;
                control_flush_next = true;
            } else if (stall) {
                IF_ID_Next = IF_ID_Curr;
                load_stall_next = true;
            } else {
                ID_EX_Next = evaluate_stage<cvsim::ID_EX_Reg>(IF_ID_Curr,
                    [this](const auto& in) { return decode(in); }, 2, IF_ID_Curr.instruction);
                const auto inst = ID_EX_Next.instruction;
                if (ID_EX_Next.fault || (ID_EX_Next.valid &&
                    (is_ebreak(inst) || (options.machine_mode && opcode(inst) == 0x73)))) {
                    fetch_stopped_next = true;
                } else if (!fetch_stopped) {
                    IF_ID_Next = fetch(pc);
                    pc_next = pc + 4;
                }
            }
        }

        void commit() {
            if (halted) return;
            if (!evaluated) throw std::logic_error("[CPU] commit() before eval()!");
            const auto cycle = stats.cycles + 1;
            if (MEM_WB_Curr.valid && !MEM_WB_Curr.fault) {
                const auto inst = MEM_WB_Curr.instruction;
                if (MEM_WB_Curr.csr_write) machine.write(inst >> 20, *MEM_WB_Curr.csr_write);
                if (MEM_WB_Curr.regWrite && MEM_WB_Curr.rd) regFile.write(MEM_WB_Curr.rd, MEM_WB_Curr.result);
                ++stats.retired;
                if (trace) trace->retire(cycle, MEM_WB_Curr);
                if (is_ebreak(inst)) halted = true;
                if (options.machine_mode && opcode(inst) == 0x73) {
                    if (inst == MRET) pc_next = machine.leave();
                    fetch_stopped_next = false;
                }
            }
            if (data_wait_next) {
                // WB may retire during the stall. Refresh held operands before EX resumes.
                ID_EX_Curr.rs1_val = regFile.read(ID_EX_Curr.rs1);
                ID_EX_Curr.rs2_val = regFile.read(ID_EX_Curr.rs2);
                MEM_WB_Curr = {};
                data_bus.clock();
                ++stats.cycles;
                ++stats.data_wait_stalls;
                evaluated = false;
                return;
            }
            if (trap_next) {
                pc_next = machine.enter(*trap_next);
                fetch_stopped_next = false;
                if (trace) trace->trap(cycle, *trap_next, pc_next);
            }
            if (MEM_WB_Next.access.write) {
                try {
                    data_bus.write();
                } catch (...) {
                    // A device can reject a store even when its address is mapped.
                    MEM_WB_Next.fault = current_fault(7, MEM_WB_Next.access.address);
                    EX_MEM_Next = {};
                    ID_EX_Next = {};
                    IF_ID_Next = {};
                    pc_next = pc;
                    fetch_stopped_next = true;
                    load_stall_next = control_flush_next = false;
                }
            }
            if (trace && MEM_WB_Next.access.width && !MEM_WB_Next.fault)
                trace->memory(cycle, MEM_WB_Next.pc, MEM_WB_Next.access);
            data_bus.clock();
            IF_ID_Curr = IF_ID_Next;
            ID_EX_Curr = ID_EX_Next;
            EX_MEM_Curr = EX_MEM_Next;
            MEM_WB_Curr = MEM_WB_Next;
            pc = pc_next;
            fetch_stopped = fetch_stopped_next;
            ++stats.cycles;
            stats.load_use_stalls += load_stall_next;
            stats.control_flushes += control_flush_next;
            evaluated = false;
        }

        void tick() { eval(); commit(); }
        void set_trace(cvsim::Trace* output) { trace = output; }
        void set_timer_interrupt(bool pending) { machine.set_timer(pending); }
        bool is_halted() const { return halted; }
        const Stats& get_stats() const { return stats; }
        cvsim::word_t read_register(cvsim::reg_addr_t addr) const { return regFile.read(addr); }
        cvsim::addr_t get_pc() const { return pc; }
        void dump_registers() const { regFile.dump(); }
};
