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

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: Caitsidhe_V <program.bin> [max_cycles]\n";
        return 1;
    }
    try {
        const auto max_cycles = argc == 3 ? std::stoull(argv[2]) : 1000000ull;
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) throw std::runtime_error("Unable to open binary!");
        const std::vector<uint8_t> program(std::istreambuf_iterator<char>{file}, {});
        RAM ram(cvsim::MEM_SIZE);
        ram.load_binary(program, 0);
        Bus bus(ram);
        CPU cpu(bus);
        while (!cpu.is_halted() && cpu.get_stats().cycles < max_cycles) cpu.tick();
        cpu.dump_registers();
        const auto& stats = cpu.get_stats();
        std::cout << "[CPU] Cycles: " << stats.cycles << " | Retired: " << stats.retired
                  << " | CPI: " << stats.cpi() << " | Load-use stalls: " << stats.load_use_stalls
                  << " | Control flushes: " << stats.control_flushes << '\n';
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
