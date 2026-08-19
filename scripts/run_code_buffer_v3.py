#!/usr/bin/env python3
"""Stress the Bootstrap v3 growable C-token buffer.

The generated source deliberately contains enough assignments to force several
geometric code-buffer reallocations and to exceed the former 65536-token
snapshot limit. The source is temporary and never becomes a repository test
artifact.
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / ".tmp" / "code_buffer_v3"
HOST = ROOT / "src" / "compiler" / "_build" / "default" / "bin" / "basaltc.exe"
BOOT_C = ROOT / "src" / "bootstrap" / "basaltc.bsl.c"
ASSIGNMENTS = 20_000
EXPECTED = f"{ASSIGNMENTS}\n"


def run(command: list[str], *, cwd: Path | None = None, capture: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        capture_output=capture,
    )


def main() -> int:
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    run(["dune", "build", "bin/basaltc.exe"], cwd=ROOT / "src" / "compiler")

    source = OUT / "code_buffer_growth.bsl"
    lines = ["func main(): int {", "  let total: int = 0;"]
    lines.extend("  total = total + 1;" for _ in range(ASSIGNMENTS))
    lines.extend(["  print total;", "  return 0;", "}", ""])
    source.write_text("\n".join(lines), encoding="utf-8")

    host_c = OUT / "host.c"
    host_bin = OUT / "host.bin"
    boot_c = OUT / "bootstrap.c"
    boot_bin = OUT / "bootstrap.bin"

    run([str(HOST), str(source)])
    generated_host_c = source.with_suffix(".bsl.c")
    shutil.copyfile(generated_host_c, host_c)
    generated_host_bin = source.with_suffix("")
    shutil.copy2(generated_host_bin, host_bin)

    run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", "-Werror", str(BOOT_C), "-o", str(OUT / "compiler.bin")])
    run([str(OUT / "compiler.bin"), str(source), str(boot_c)])
    run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", "-Werror", str(boot_c), "-o", str(boot_bin)])

    host_output = subprocess.run([str(host_bin)], check=True, text=True, capture_output=True).stdout
    boot_output = subprocess.run([str(boot_bin)], check=True, text=True, capture_output=True).stdout
    if host_output != EXPECTED or boot_output != EXPECTED:
        raise SystemExit(f"output mismatch: expected {EXPECTED!r}, host={host_output!r}, bootstrap={boot_output!r}")
    if host_output != boot_output:
        raise SystemExit("Host/Bootstrap output parity failed")

    host_size = host_c.stat().st_size
    boot_size = boot_c.stat().st_size
    if host_size <= 65_536 or boot_size <= 65_536:
        raise SystemExit(f"generated C did not cross former buffer threshold: Host={host_size}, Bootstrap={boot_size}")

    print(f"PASS code_buffer_v3: {ASSIGNMENTS} assignments, Host/Bootstrap output={ASSIGNMENTS}, C sizes Host={host_size} Bootstrap={boot_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
