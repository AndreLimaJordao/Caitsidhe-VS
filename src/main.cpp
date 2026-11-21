#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <exception>
#include "common/types.h"
#include "core/cpu.h"
#include "memory/ram.h"
#include "memory/bus.h"
#include "platform/board.h"
#include "platform/runner_options.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: Caitsidhe_V <program.bin> [max_cycles] [--trace file] [--wait-states n] [--machine]\n";
        return 1;
    }
    try {
        auto max_cycles = 1000000ull;
        RunnerOptions options;
        bool has_limit = false;
        for (int i = 2; i < argc; ++i) {
            if (options.parse(i, argc, argv)) continue;
            const std::string arg = argv[i];
            if (has_limit || arg.empty() || arg[0] == '-')
                throw std::invalid_argument("Unknown or incomplete argument: " + arg);
            size_t parsed = 0;
            max_cycles = std::stoull(arg, &parsed);
            if (parsed != arg.size()) throw std::invalid_argument("Invalid cycle limit");
            has_limit = true;
        }
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) throw std::runtime_error("Unable to open binary!");
        const std::vector<uint8_t> program(std::istreambuf_iterator<char>{file}, {});
        Board board(options.cpu);
        SoC& soc = board.soc();
        std::ofstream trace_file;
        options.open_trace(trace_file, argv[1]);
        cvsim::Trace trace(trace_file);
        if (trace_file.is_open()) soc.cpu().set_trace(&trace);
        soc.load(program);
        const CPU& cpu = soc.cpu();
        while (!cpu.is_halted() && cpu.get_stats().cycles < max_cycles) soc.tick();
        if (trace_file.is_open()) trace_file.flush();
        std::cout << board.uart_output();
        cpu.dump_registers();
        const auto& stats = cpu.get_stats();
        std::cout << "[CPU] Cycles: " << stats.cycles 
                  << " | Retired: " << stats.retired
                  << " | CPI: " << stats.cpi() 
                  << " | Load-use stalls: " << stats.load_use_stalls
                  << " | Control flushes: " << stats.control_flushes
                  << " | Data wait stalls: " << stats.data_wait_stalls << '\n';
        if (!cpu.is_halted()) {
            std::cerr << "[CPU] Cycle limit reached without EBREAK.\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Caitsidhe-VS] " << e.what() << '\n';
        return 1;
    }
}
