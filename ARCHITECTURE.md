# Architectural exploration layer

The Caitsidhe-VS is a pipeline model for prototyping SoC configurations that can later be translated into Caitsidhe-V RTL. It models a Wishbone Classic data port with configurable latency. Cycle-by-cycle comparison with the RTL remains a separate milestone.

## Subsystems

- `src/core/`: RV32I in-order five-stage CPU, ISA decode, forwarding, pipeline state.
- `src/memory/`: byte-addressable RAM, address decoding and a Wishbone Classic data port.
- `src/peripherals/`: lightweight device contract, register bank, UART TX, GPIO, Timer and CRC32.
- `src/platform/`: `SoC` integration facade and the default board shared by both runners.
- `tests/`: CPU and MMIO regressions, firmware demos and RV32I differential tests.

## Design constraints and timing

Each peripheral implements `read(offset)` and `write(offset, data, wstrb)`.
The offset is a byte offset relative to the peripheral base. Transfers are single aligned 32-bit words. Byte/halfword operations are represented as lane masks and right-shifted read results at the Bus/CPU boundary. 

Device reads happen once per load. A store validates its mapped address during CPU evaluation without doing a speculative MMIO read. Its write occurs during CPU `commit()`.

The CPU carries faults with the affected instructions until writeback. An older redirect can discard a pending fetch fault. A fault on the executed path stops younger instructions and lets older ones finish committing.

The CPU fetches instructions directly from RAM. Loads and stores use a single-outstanding, single-beat Wishbone Classic data port. It holds `CYC`, `STB`, address, direction, data and byte selects stable until `ACK` or `ERR`. Each configured wait state holds EX/MEM and the younger stages for one extra cycle. An instruction already in WB can retire during that wait. Each device access occurs once, on the completing beat.

The bus decodes the address and calls the same `Peripheral` API on completion. Latency is uniform across data accesses. Pipelined Wishbone, bursts, DMA and arbitration are future work.

`Bus::attach()` and `SoC::attach()` hold a non-owning reference. The device must outlive the bus. Mapping ranges must be four-byte aligned, nonempty, nonoverlapping and contained in 32-bit address space. RAM occupies `0x80000000` through `0x800fffff` by default. A peripheral may be mapped at `0x90000000` or any other disjoint region.

`SoC::tick()` samples the timer interrupt, advances the CPU and calls `tick()` once per device attached through `SoC::attach()`, after the CPU commits its writes for that cycle. The command-line runners instantiate the board described in the [README](README.md#memory-map). A plain `SoC` starts with RAM and accepts its own MMIO map.

## Machine mode

The runners enable the machine-mode subset with `--machine`. It implements CSR instructions for `mstatus`, `misa`, `mie`, `mtvec`, `mscratch`, `mepc`, `mcause`, `mtval`, `mip` and `mhartid`, plus `ECALL`, `EBREAK` and `MRET`. Traps update the machine CSRs and redirect to `mtvec`. The board's 32-bit Timer drives the machine timer interrupt level through its `COUNT` and `COMPARE` registers. It keeps its original custom MMIO layout.

Interrupts are sampled between instructions. An older writeback can finish at the interrupt boundary, and an active data transaction completes before the CPU takes the interrupt. Default runs retain host errors and the legacy `EBREAK` stop marker.

## Minimal peripheral example

```cpp
#include "platform/soc.h"
#include "peripherals/register_bank.h"

int main() {
    RegisterBank gpio(4);
    SoC soc;
    soc.attach(0x90000000, 16, gpio);
    soc.bus().write(0x90000000, 0x55, 0xf);
    return soc.bus().read(0x90000000) == 0x55 ? 0 : 1;
}
```

For behavior beyond simple stored registers, derive from `Peripheral` and implement its two virtual functions. `wstrb` is a bit mask for byte lanes, `0b0100` updates only bits 16..23. Data has already been shifted into the correct lane by the CPU. An MMIO read must be deterministic at the point when the CPU invokes it, but it may intentionally have side effects.

## Mapping a VS model to RTL

A peripheral model should document: register offsets and semantics, reset values, access widths, byte enables, interrupt behavior, and any intended latency/throughput characteristics. 

The RTL peripheral can then implement the same external register contract. Compare architectural results and MMIO transaction sequences first, and only compare exact cycles in components that explicitly claim cycle accuracy.

## Trace and verification

With `--trace`, each line is a JSON object. Fields use integer values for addresses, instructions and data.

- `retire` reports the instruction PC and encoding, destination register and value when applicable, and its data access if any.
- `memory` reports a completed load or store at its bus cycle. Each access records byte address, width, direction and unsigned lane value before sign extension. A terminating `tohost` store can appear here before the runner stops.
- `trap` reports the saved PC, cause, trap value and handler target.

The [reference comparison](README.md#compare-against-an-independent-emulator) uses `retire` events to compare the same RAM-only RV32I ELF against a live Unicorn run. The test examines architectural state at each step and uses `EBREAK` as the common stop marker. The IRQ firmware and Wishbone unit tests verify the local board and transport. ACT4 integration and RTL cycle validation are on the roadmap.

