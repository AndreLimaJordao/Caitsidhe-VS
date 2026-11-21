#include "program/elf_loader.h"
#include "platform/board.h"
#include "platform/runner_options.h"
#include <exception>
#include <cstdio>
#include <iterator>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: caitsidhe_elf test.elf [max_cycles] [--no-tohost] [--signature-output file] [--reference file] [--trace file] [--wait-states n] [--machine]\n";
        return 2;
    }
    try {
        uint64_t limit=100000;
        bool use_tohost=true;
        RunnerOptions options;
        std::string output_path, reference_path;
        for (int i=2; i<argc; ++i) {
            const std::string a=argv[i];
            if (options.parse(i, argc, argv)) continue;
            if (a=="--no-tohost") use_tohost=false;
            else if (a=="--signature-output" && i+1<argc) output_path=argv[++i];
            else if (a=="--reference" && i+1<argc) reference_path=argv[++i];
            else if (a.size() && a[0]=='-') throw std::invalid_argument("Unknown or incomplete argument: " + a);
            else limit=std::stoull(a);
        }
        Board board(options.cpu);
        SoC& soc = board.soc();
        const auto image=elf32::load(argv[1],soc.ram());
        std::ofstream trace_file;
        options.open_trace(trace_file, argv[1]);
        cvsim::Trace trace(trace_file);
        if (trace_file.is_open()) soc.cpu().set_trace(&trace);
        const auto host=image.symbols.find("tohost");
        const bool monitor=use_tohost && host != image.symbols.end();
        const uint32_t host_address=monitor ? host->second : 0;
        const uint64_t ram_begin=MemoryMap::RAM_BASE;
        const uint64_t ram_end=ram_begin+soc.ram().size();
        if (monitor && (host_address & 3u)) throw std::runtime_error("Unaligned tohost");
        if (monitor && (host_address < ram_begin || uint64_t(host_address)+4 > ram_end))
            throw std::runtime_error("tohost outside RAM");
        int completion=2;
        std::string verdict="TIMEOUT";
        for (uint64_t i=0; i<limit; ++i) {
            soc.tick();
            if (monitor) {
                const uint32_t value=soc.bus().read(host_address);
                if (value != 0) {
                    completion=value==1 ? 0 : 1;
                    verdict=value==1 ? "PASS (tohost=1)" : "FAIL (tohost=" + std::to_string(value) + ")";
                    break;
                }
            }
            if (soc.cpu().is_halted()) {
                completion=monitor ? 2 : 0;
                verdict=monitor ? "INCOMPLETE (EBREAK before tohost)" : "PASS (EBREAK termination; not an ISA correctness oracle)";
                break;
            }
        }
        if (trace_file.is_open()) trace_file.flush();
        if (!output_path.empty() || !reference_path.empty()) {
            const auto begin=image.symbols.find("begin_signature");
            const auto end=image.symbols.find("end_signature");
            if (begin==image.symbols.end() || end==image.symbols.end())
                throw std::runtime_error("Signature symbols begin_signature/end_signature not found");
            if (end->second < begin->second || (begin->second & 3u) || (end->second & 3u) ||
                begin->second < ram_begin || end->second > ram_end)
                throw std::runtime_error("Invalid signature region");
            std::string signature;
            for (uint64_t a=begin->second; a<end->second; a+=4) {
                const uint32_t word=soc.bus().read(static_cast<uint32_t>(a));
                char buf[16];
                std::snprintf(buf,sizeof(buf),"%08x\n",word);
                signature += buf;
            }
            if (!output_path.empty()) {
                std::ofstream out(output_path);
                if (!out) throw std::runtime_error("Cannot create signature file");
                out << signature;
            }
            if (!reference_path.empty()) {
                std::ifstream in(reference_path);
                if (!in) throw std::runtime_error("Cannot open signature reference");
                std::string expected((std::istreambuf_iterator<char>(in)), {});
                if (signature != expected) {
                    verdict="FAIL (signature mismatch)";
                    completion=1;
                } else if (completion==0) {
                    verdict="PASS (termination and signature matched)";
                }
            }
        }
        const auto& uart = board.uart_output();
        if (!uart.empty()) {
            std::cout << uart;
            if (uart.back() != '\n') std::cout << '\n';
            std::cout.flush();
        }
        (completion == 0 ? std::cout : std::cerr) << "RESULT: " << verdict << '\n';
        return completion;
    } catch (const std::exception& e) {
        std::cerr << "RESULT: ERROR (" << e.what() << ")\n";
        return 2;
    }
}
