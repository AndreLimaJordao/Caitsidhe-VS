#pragma once
#include "../memory/ram.h"
#include "../common/types.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace elf32 {
struct Image {
    uint32_t entry = 0;
    std::map<std::string, uint32_t> symbols;
};
inline uint16_t u16(const std::vector<uint8_t>& d, uint64_t p) {
    if (p > d.size() || d.size() - p < 2) throw std::runtime_error("[ELF] Truncated u16");
    return uint16_t(d[p]) | uint16_t(d[p+1]) << 8;
}
inline uint32_t u32(const std::vector<uint8_t>& d, uint64_t p) {
    if (p > d.size() || d.size() - p < 4) throw std::runtime_error("[ELF] Truncated u32");
    return uint32_t(d[p]) | uint32_t(d[p+1]) << 8 | uint32_t(d[p+2]) << 16 | uint32_t(d[p+3]) << 24;
}
inline void range(const std::vector<uint8_t>& d, uint64_t p, uint64_t n) {
    if (p > d.size() || n > d.size()-p) throw std::runtime_error("[ELF] Truncated file");
}
inline std::string name(const std::vector<uint8_t>& d, uint64_t start, uint64_t size, uint32_t offset) {
    if (offset >= size) throw std::runtime_error("[ELF] Bad string table offset");
    range(d, start, size);
    std::string out;
    for (uint64_t i=start+offset; i<start+size && d[i]; ++i) out.push_back(char(d[i]));
    return out;
}
inline Image load(const std::string& filename, RAM& ram) {
    std::ifstream input(filename, std::ios::binary);
    if (!input) throw std::runtime_error("[ELF] Cannot open file: " + filename);
    const std::vector<uint8_t> d(std::istreambuf_iterator<char>{input}, {});
    range(d, 0, 52);
    if (d[0]!=0x7f || d[1]!='E' || d[2]!='L' || d[3]!='F' || d[4]!=1 || d[5]!=1 || d[6]!=1)
        throw std::runtime_error("[ELF] Expected ELF32 little-endian");
    if (u16(d, 18) != 243) throw std::runtime_error("[ELF] Not RISC-V machine");
    if (u16(d, 16) != 2) throw std::runtime_error("[ELF] Expected executable ET_EXEC");
    Image result;
    result.entry = u32(d, 24);
    if (result.entry != cvsim::RESET_VECTOR)
        throw std::runtime_error("[ELF] Entry must equal current fixed reset vector (0x80000000)");
    const uint32_t phoff = u32(d, 28), shoff = u32(d, 32);
    const uint16_t phentsize = u16(d, 42), phnum = u16(d, 44);
    const uint16_t shentsize = u16(d, 46), shnum = u16(d, 48);
    if (phentsize < 32 && phnum) throw std::runtime_error("[ELF] Invalid program header size");
    bool loaded = false;
    for (unsigned i=0; i<phnum; ++i) {
        const uint64_t p = uint64_t(phoff)+uint64_t(i)*phentsize;
        range(d, p, phentsize);
        if (u32(d,p) != 1) continue;
        const uint32_t fileoff=u32(d,p+4), vaddr=u32(d,p+8), filesz=u32(d,p+16), memsz=u32(d,p+20);
        if (filesz>memsz) throw std::runtime_error("[ELF] Invalid segment sizes");
        range(d,fileoff,filesz);
        if (vaddr < cvsim::RESET_VECTOR || uint64_t(vaddr-cvsim::RESET_VECTOR)+memsz > ram.size())
            throw std::runtime_error("[ELF] Loadable segment outside RAM");
        const size_t base=vaddr-cvsim::RESET_VECTOR;
        std::vector<uint8_t> bytes(memsz,0);
        for (uint32_t j=0; j<filesz; ++j) bytes[j]=d[fileoff+j];
        ram.load_binary(bytes, static_cast<uint32_t>(base));
        loaded=true;
    }
    if (!loaded) throw std::runtime_error("[ELF] No PT_LOAD segments");
    if (shnum && shentsize < 40) throw std::runtime_error("[ELF] Invalid section header size");
    for (unsigned i=0; i<shnum; ++i) {
        const uint64_t sh=uint64_t(shoff)+uint64_t(i)*shentsize;
        range(d,sh,shentsize);
        if (u32(d,sh+4)!=2) continue; // SHT_SYMTAB
        const uint32_t offset=u32(d,sh+16), size=u32(d,sh+20), link=u32(d,sh+24), entsize=u32(d,sh+36);
        if (entsize<16 || link>=shnum) throw std::runtime_error("[ELF] Invalid symbol table");
        const uint64_t string_header=uint64_t(shoff)+uint64_t(link)*shentsize;
        range(d,string_header,shentsize);
        const uint32_t str_off=u32(d,string_header+16), str_size=u32(d,string_header+20);
        range(d,offset,size); range(d,str_off,str_size);
        for (uint64_t e=offset; e+entsize<=uint64_t(offset)+size; e+=entsize) {
            const uint32_t sym_name=u32(d,e), value=u32(d,e+4);
            if (sym_name && u16(d,e+14)!=0) result.symbols[name(d,str_off,str_size,sym_name)]=value;
        }
    }
    return result;
}
}
