"""
Build, run, and report every configured local CTest plus optional external ELFs.
Requires Python 3.9+, CMake and C++ compiler. Clang/llvm-objcopy unlock the
independent RISC-V assembly tests. No upstream suite is bundled or claimed.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def run(cmd, cwd=None):
    print("+", " ".join(map(str, cmd)), flush=True)
    proc = subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, errors="replace")
    print(proc.stdout[-12000:], flush=True)
    return proc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", type=Path, default=ROOT / "build")
    ap.add_argument("--report-dir", type=Path, default=ROOT / "reports")
    ap.add_argument("--external-dir", type=Path, help="Optional prebuilt RV32 ELF files; NOT automatically ACT4 certification")
    ap.add_argument("--max-cycles", type=int, default=100000)
    args = ap.parse_args()
    build = args.build_dir.resolve()
    report = args.report_dir.resolve()
    report.mkdir(parents=True, exist_ok=True)
    tests = []
    config = run(["cmake", "-S", str(ROOT), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_TESTING=ON"])
    compile_result = run(["cmake", "--build", str(build), "--parallel", "2"]) if config.returncode == 0 else None
    if config.returncode != 0 or compile_result.returncode != 0:
        tests.append({"name": "configure/build", "kind": "build", "status": "FAIL", "detail": "CMake failed"})
    else:
        tests.append({"name": "configure/build", "kind": "build", "status": "PASS", "detail": "Build successful"})
        junit = report / "ctest.xml"
        proc = run(["ctest", "--test-dir", str(build), "--output-on-failure", "--output-junit", str(junit)])
        if junit.exists():
            tree = ET.parse(junit)
            for entry in tree.iter("testcase"):
                failed = entry.find("failure") is not None or entry.find("error") is not None
                skipped = entry.find("skipped") is not None
                tests.append({"name": entry.get("name", "unnamed"), "kind": "local-ctest",
                              "status": "SKIP" if skipped else "FAIL" if failed else "PASS",
                              "detail": "See ctest.xml for output"})
        # Make missing optional toolchains visible instead of silently implying coverage.
        expected_optional = ["caitsidhe_rv32i_binary", "caitsidhe_tohost_pass",
                             "caitsidhe_signature_match", "caitsidhe_tohost_fail_expected"]
        observed = {t["name"] for t in tests}
        for missing in expected_optional:
            if missing not in observed:
                tests.append({"name": missing, "kind": "local-ctest", "status": "SKIP",
                              "detail": "Not registered: missing optional build dependencies"})
        if proc.returncode != 0 and not any(t["kind"] == "local-ctest" and t["status"] == "FAIL" for t in tests):
            tests.append({"name": "ctest-runner", "kind": "local-ctest", "status": "FAIL", "detail": "CTest returned nonzero"})
        if args.external_dir is not None:
            ext = args.external_dir.resolve()
            files = sorted(ext.rglob("*.elf")) if ext.is_dir() else []
            if not files:
                tests.append({"name": "external ELF selection", "kind": "external-elf", "status": "SKIP", "detail": "No .elf files found"})
            for elf in files:
                started = time.monotonic()
                try:
                    res = subprocess.run([str(build / "caitsidhe_elf"), str(elf), str(args.max_cycles)],
                                         capture_output=True, text=True, errors="replace", timeout=60)
                    detail = (res.stdout + res.stderr).strip()[-2000:]
                    # EBREAK alone is not a correctness oracle for external tests.
                    status = "PASS" if res.returncode == 0 and "PASS (tohost=1)" in detail else (
                        "UNVERIFIED" if res.returncode == 0 else "FAIL")
                except subprocess.TimeoutExpired:
                    status, detail = "FAIL", "Host execution timeout (60 seconds)"
                tests.append({"name": elf.name, "kind": "external-elf", "status": status,
                              "detail": detail, "duration_seconds": round(time.monotonic() - started, 3)})
    counts = {s: sum(x["status"] == s for x in tests) for s in ["PASS", "FAIL", "SKIP", "UNVERIFIED"]}
    document = {"schema": 1, "suite": "Caitsidhe-VS", "upstream_act4_certified": False,
                "upstream_riscv_tests_executed": False,
                "external_elf_directory_requested": args.external_dir is not None,
                "counts": counts, "tests": tests}
    (report / "results.json").write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n")
    lines = ["# Caitsidhe-VS — verificação", "", "**Resultados da execução atual, não certificação RISC-V.**", "",
             f"PASS: **{counts['PASS']}** · FAIL: **{counts['FAIL']}** · SKIP: **{counts['SKIP']}** · UNVERIFIED: **{counts['UNVERIFIED']}**", "",
             "| Teste | Origem | Estado |", "|---|---|---|"]
    for t in tests:
        lines.append("| " + t["name"].replace("|", "\\|") + " | " + t["kind"] + " | " + t["status"] + " |")
    lines += ["", "ACT4: **não integrado**. `external-elf` só atesta êxito com `tohost=1`; EBREAK isolado é `UNVERIFIED`.", ""]
    (report / "summary.md").write_text("\n".join(lines), encoding="utf-8")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as out:
            out.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 1 if counts["FAIL"] or counts["UNVERIFIED"] else 0


if __name__ == "__main__":
    sys.exit(main())
