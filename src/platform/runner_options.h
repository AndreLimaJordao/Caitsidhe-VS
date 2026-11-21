#pragma once
#include "../core/cpu.h"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <string_view>

struct RunnerOptions {
    CPU::Options cpu;
    std::string trace_path;

    bool parse(int& index, int argc, char** argv) {
        const std::string_view option = argv[index];
        if (option == "--machine") {
            cpu.machine_mode = true;
        } else if (option == "--trace" && index + 1 < argc) {
            trace_path = argv[++index];
        } else if (option == "--wait-states" && index + 1 < argc) {
            const std::string_view value = argv[++index];
            const auto end = value.data() + value.size();
            const auto result = std::from_chars(value.data(), end, cpu.wait_states);
            if (result.ec != std::errc{} || result.ptr != end)
                throw std::invalid_argument("Invalid wait-state count");
        } else return false;
        return true;
    }

    void open_trace(std::ofstream& out, const char* program) const {
        if (trace_path.empty()) return;
        std::error_code error;
        if (std::filesystem::equivalent(trace_path, program, error))
            throw std::invalid_argument("Trace and program must use different files");
        out.open(trace_path);
        if (!out) throw std::runtime_error("Cannot create trace file");
        out.exceptions(std::ios::badbit | std::ios::failbit);
    }
};
