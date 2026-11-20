# Architectural exploration layer

The Caitsidhe-VS is a zero-wait-state pipeline model, meant to prototype SoC configurations that can later be translated into Caitsidhe-V RTL. It is not cycle-equivalent to RTL or a Wishbone-compliant interconnect yet.

## Subsystems

- `src/core/`: RV32I in-order five-stage CPU, ISA decode, forwarding, pipeline state.
- `src/memory/`: byte-addressable RAM and address-decoding synchronous transaction bus.
- `src/peripherals/`: lightweight device contract and a ready-made register bank.
- `src/platform/`: `SoC` integration facade to compose CPU, bus, RAM and user devices.
- `tests/`: CPU smoke tests, real pipeline MMIO tests and an independent RV32I assembly binary.

## Design constraints and timing

Each peripheral implements `read(offset)` and `write(offset, data, wstrb)`.
The offset is a byte offset relative to the peripheral base. Transfers are single aligned 32-bit words. Byte/halfword operations are represented as lane masks and right-shifted read results at the Bus/CPU boundary. 

Device reads happen once per load. A store validates its mapped address during CPU evaluation without doing a speculative MMIO read, its write occurs during CPU `commit()`

No combinational read/write handshake, wait states, DMA, arbitration, reset broadcast, interrupts or Wishbone error responses are currently modeled.
Do not confuse this API with Wishbone B4's signal-level protocol. In a later version a Wishbone adapter could implement this same user-facing `Peripheral` API for zero-wait-state devices, with an extended async API for delayed transactions. Keeping transport and functionality separate is intentional.

`Bus::attach()` and `SoC::attach()` hold a non-owning reference, the device must outlive the bus. Mapping ranges must be four-byte aligned, nonempty, nonoverlapping and contained in 32-bit address space. RAM occupies `0x80000000` through `0x800fffff` by default. A peripheral may be mapped at `0x90000000` or any other disjoint region.

## Minimal peripheral example

```cpp
#include "platform/soc.h"
#include "peripherals/register_bank.h"

int main() {
    SoC soc;
    RegisterBank gpio(4);                  
    soc.attach(0x90000000, 16, gpio);      
    soc.bus().write(0x90000000, 0x55, 0xf);
    return soc.bus().read(0x90000000) == 0x55 ? 0 : 1;
}
```

For behavior beyond simple stored registers, derive from `Peripheral` and implement its two virtual functions. `wstrb` is a bit mask for byte lanes, `0b0100` updates only bits 16..23. Data has already been shifted into the correct lane by the CPU. An MMIO read must be deterministic at the point when the CPU invokes it, but it may intentionally have side effects.

## Mapping a VS model to RTL

A peripheral model should document: register offsets and semantics, reset values, access widths, byte enables, interrupt behavior, and any intended latency/throughput characteristics. 

The RTL peripheral can then implement the same external register contract. Compare architectural results and MMIO transaction sequences first, and only compare exact cycles in components that explicitly claim cycle accuracy.

## RISC-V upstream verification roadmap

The project currently verifies RV32I through its own instruction-level smoke
tests and a bare-metal RV32I assembly regression assembled by Clang (when
available). These are not official RISC-V architecture tests.
**No ACT4 or upstream `riscv-tests` pass claim is made at this stage.**

