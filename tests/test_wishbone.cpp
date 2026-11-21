#include "../src/memory/wishbone.h"
#include "../src/platform/board.h"
#include "check.h"

static uint64_t run(unsigned wait_states) {
    Board board({wait_states, false});
    auto& soc = board.soc();
    // UART write and read, with x3 reaching WB while the store waits.
    // lui x1,UART; li x2,65; li x3,37; sb x2,0(x1)
    // addi x4,x3,5; lw x5,4(x1); add x6,x5,x4; ebreak
    const uint32_t words[] = {0x900000b7, 0x04100113, 0x02500193, 0x00208023,0x00518213, 0x0040a283, 0x00428333, 0x00100073};
    for (unsigned i = 0; i < std::size(words); ++i) soc.ram().write_word_masked(i * 4, words[i], 15);
    for (unsigned i = 0; i < 100 && !soc.cpu().is_halted(); ++i) soc.tick();
    CHECK(soc.cpu().is_halted());
    CHECK(board.uart_output() == "A");
    CHECK(soc.cpu().read_register(4) == 42);
    CHECK(soc.cpu().read_register(5) == 1);
    CHECK(soc.cpu().read_register(6) == 43);
    CHECK(soc.cpu().get_stats().retired == std::size(words));
    CHECK(soc.cpu().get_stats().data_wait_stalls == 2 * wait_states);
    return soc.cpu().get_stats().cycles;
}

int main() {
    RAM ram(8);
    Bus bus(ram);
    Wishbone link(bus, 1);
    const auto addr = MemoryMap::RAM_BASE + 1;
    CHECK(!link.request(addr, 1, true, 0xab));
    CHECK(link.signals().cyc && link.signals().stb && link.signals().we);
    CHECK(link.signals().adr == MemoryMap::RAM_BASE && link.signals().sel == 2);
    CHECK(link.signals().dat == 0xab00 && !link.signals().ack);
    CHECK(ram.read_word_aligned(0) == 0);
    link.clock();
    CHECK(link.request(addr, 1, true, 0xab));
    link.write();
    CHECK(link.signals().ack && !link.signals().err);
    link.clock();
    CHECK(!link.signals().cyc && !link.signals().stb);
    CHECK(ram.read_word_aligned(0) == 0xab00);
    CHECK(!link.request(addr, 1, false, 0));
    link.clock();
    CHECK(link.request(addr, 1, false, 0));
    CHECK(link.read() == 0xab && link.signals().dat_r == 0xab00 && link.signals().ack);
    link.clock();
    CHECK(!link.request(0x40000000, 4, false, 0));
    link.clock();
    CHECK(link.request(0x40000000, 4, false, 0));
    bool caught = false;
    try { link.read(); } catch (const std::out_of_range&) { caught = true; }
    CHECK(caught && link.signals().err && !link.signals().ack);
    link.clock();
    CHECK(!link.signals().cyc);

    const auto base = run(0);
    CHECK(run(1) == base + 2);
    CHECK(run(5) == base + 10);
}
