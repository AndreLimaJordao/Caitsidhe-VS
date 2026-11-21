"""Compare a RAM-only RV32I ELF against an independent Unicorn execution."""

import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from unicorn import (
    UC_ARCH_RISCV, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_MEM_WRITE,
    UC_MODE_RISCV32, Uc, UcError, __version__,
)
from unicorn import riscv_const


RAM_BASE = 0x80000000
RAM_SIZE = 1024 * 1024
EBREAK = 0x00100073
REGISTERS = [getattr(riscv_const, f"UC_RISCV_REG_X{i}") for i in range(1, 32)]


def load_elf(machine, path):
    binary = path.read_bytes()
    if binary[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Expected a little-endian ELF32")
    kind, arch, _, entry, headers, _, flags, _, stride, count = struct.unpack_from(
        "<HHIIIIIHHH", binary, 16
    )
    if (kind, arch, entry) != (2, 243, RAM_BASE) or flags & 1:
        raise ValueError("Expected an uncompressed RISC-V executable at 0x80000000")
    if stride < 32:
        raise ValueError("Invalid program header size")
    machine.mem_map(RAM_BASE, RAM_SIZE)
    loaded = False
    for index in range(count):
        kind, offset, address, _, file_size, mem_size, _, _ = struct.unpack_from(
            "<8I", binary, headers + index * stride
        )
        if kind != 1 or not mem_size:
            continue
        if (file_size > mem_size or offset + file_size > len(binary)
                or address < RAM_BASE or address + mem_size > RAM_BASE + RAM_SIZE):
            raise ValueError("Loadable segment is outside the file or simulator RAM")
        machine.mem_write(address, bytes(mem_size))
        if file_size:
            machine.mem_write(address, binary[offset:offset + file_size])
        loaded = True
    if not loaded:
        raise ValueError("ELF has no loadable RAM segments")
    machine.reg_write(riscv_const.UC_RISCV_REG_PC, entry)


def compare(elf, trace_path, max_instructions):
    machine = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
    load_elf(machine, elf)
    transactions = []

    def access(uc, kind, address, width, value, _):
        write = kind == UC_MEM_WRITE
        if write:
            value &= (1 << (8 * width)) - 1
        else:
            value = int.from_bytes(uc.mem_read(address, width), "little")
        transactions.append(dict(address=address, width=width, write=write, value=value))

    machine.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, access)
    caitsidhe_registers = [0] * 31
    retired = 0
    stopped = False
    with trace_path.open(encoding="utf-8") as trace:
        for line in trace:
            row = json.loads(line)
            if row["event"] != "retire":
                if row["event"] == "trap":
                    raise ValueError(f"Unexpected trap in RAM-only comparison: {row}")
                continue
            retired += 1
            if stopped or retired > max_instructions:
                raise ValueError("Trace continued past EBREAK or the instruction limit")
            pc = machine.reg_read(riscv_const.UC_RISCV_REG_PC)
            inst = int.from_bytes(machine.mem_read(pc, 4), "little")
            where = f"instruction {retired} at 0x{pc:08x}"
            if (row["pc"], row["instruction"]) != (pc, inst):
                raise ValueError(
                    f"{where}: Unicorn fetched 0x{inst:08x}, Caitsidhe fetched "
                    f"0x{row['instruction']:08x} at 0x{row['pc']:08x}"
                )
            transactions.clear()
            if inst == EBREAK:
                stopped = True  # Same stop marker as the default Caitsidhe runner.
            else:
                if inst & 3 != 3:
                    raise ValueError(f"{where}: expected a 32-bit instruction")
                try:
                    machine.emu_start(pc, 0, count=1)
                except UcError as error:
                    raise ValueError(f"{where}: Unicorn: {error}") from error
            observed = [] if row["memory"] is None else [row["memory"]]
            if transactions != observed:
                raise ValueError(f"{where}: Unicorn memory {transactions}, Caitsidhe memory {observed}")
            if row["rd"] is not None:
                if not 1 <= row["rd"] <= 31:
                    raise ValueError(f"{where}: invalid destination register {row['rd']}")
                caitsidhe_registers[row["rd"] - 1] = row["value"]
            for index, register in enumerate(REGISTERS):
                expected = machine.reg_read(register)
                actual = caitsidhe_registers[index]
                if expected != actual:
                    raise ValueError(
                        f"{where}: x{index + 1} Unicorn=0x{expected:08x}, Caitsidhe=0x{actual:08x}"
                    )
    if not stopped or retired < 2:
        raise ValueError("Expected a completed run with at least one instruction before EBREAK")
    return retired


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--sim", type=Path, help="Run this caitsidhe_elf executable")
    inputs.add_argument("--trace", type=Path, help="Compare a previously saved JSONL trace")
    parser.add_argument("--wait-states", type=int, default=0)
    parser.add_argument("--max-cycles", type=int, default=200_000)
    parser.add_argument("--max-instructions", type=int, default=20_000)
    args = parser.parse_args()
    if args.wait_states < 0 or args.max_cycles < 1 or args.max_instructions < 2:
        parser.error("Invalid wait states or execution limit")
    try:
        if args.sim:
            with tempfile.TemporaryDirectory(prefix="caitsidhe-reference-") as directory:
                trace_path = Path(directory) / "trace.jsonl"
                proc = subprocess.run(
                    [str(args.sim.resolve()), str(args.elf), str(args.max_cycles), "--no-tohost",
                     "--wait-states", str(args.wait_states), "--trace", str(trace_path)],
                    capture_output=True, text=True, errors="replace", timeout=60,
                )
                if proc.returncode != 0:
                    raise ValueError(f"Caitsidhe exited {proc.returncode}:\n{proc.stdout}{proc.stderr}")
                count = compare(args.elf, trace_path, args.max_instructions)
        else:
            count = compare(args.elf, args.trace, args.max_instructions)
    except (OSError, ValueError, KeyError, struct.error, UcError, subprocess.TimeoutExpired) as error:
        print(f"Reference comparison failed: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {count} matched RV32I instructions against Unicorn {__version__} (including EBREAK marker)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
