#include "program/elf_loader.h"
#include "platform/soc.h"
#include <exception>
#include <cstdio>
#include <iterator>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: caitsidhe_elf test.elf [max_cycles] [--no-tohost]"
                     " [--signature-output file] [--reference file]\n";
        return 2;
    }
    try {
        uint64_t limit=100000;
        bool use_tohost=true;
        std::string output_path, reference_path;
        for (int i=2; i<argc; ++i) {
            const std::string a=argv[i];
            if (a=="--no-tohost") use_tohost=false;
            else if (a=="--signature-output" && i+1<argc) output_path=argv[++i];
            else if (a=="--reference" && i+1<argc) reference_path=argv[++i];
            else if (a.size() && a[0]=='-') throw std::invalid_argument("Unknown or incomplete argument: " + a);
            else limit=std::stoull(a);
        }
        SoC soc;
        const auto image=elf32::load(argv[1],soc.ram());
        const auto host=image.symbols.find("tohost");
        const bool monitor=use_tohost && host != image.symbols.end();
        const uint32_t host_address=monitor ? host->second : 0;
        if (monitor && (host_address & 3u)) throw std::runtime_error("Unaligned tohost");
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
        if (!output_path.empty() || !reference_path.empty()) {
            const auto begin=image.symbols.find("begin_signature");
            const auto end=image.symbols.find("end_signature");
            if (begin==image.symbols.end() || end==image.symbols.end())
                throw std::runtime_error("Signature symbols begin_signature/end_signature not found");
            if (end->second < begin->second || (begin->second & 3u) || (end->second & 3u) ||
                uint64_t(end->second)-begin->second > soc.ram().size())
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
        (completion == 0 ? std::cout : std::cerr) << "RESULT: " << verdict << '\n';
        return completion;
    } catch (const std::exception& e) {
        std::cerr << "RESULT: ERROR (" << e.what() << ")\n";
        return 2;
    }
}
