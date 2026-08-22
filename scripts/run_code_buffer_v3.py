"""Stress the Bootstrap v3 growable C-token buffer.

The generated source deliberately contains enough assignments to force several
geometric code-buffer reallocations and to exceed the former 65536-token
snapshot limit. The source is temporary and never becomes a repository test
artifact. The stored C compiler creates the current Bootstrap compiler; the workload is
compiled only by that current-generation binary.
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / ".tmp" / "code_buffer_v3"
BOOTSTRAP_STAGE = ROOT / "scripts" / "bootstrap_stage.sh"
ASSIGNMENTS = 20_000
EXPECTED = f"{ASSIGNMENTS}\n"
STRICT = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", "-Werror"]


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

    source = OUT / "code_buffer_growth.basalt"
    lines = ["func main(): int {", "  let total: int = 0;"]
    lines.extend("  total = total + 1;" for _ in range(ASSIGNMENTS))
    lines.extend(["  println total;", "  return 0;", "}", ""])
    source.write_text("\n".join(lines), encoding="utf-8")

    stage_out = OUT / "bootstrap-stage"
    boot_c = OUT / "workload.c"

    stage_result = run(
        ["bash", "-c", "source \"$1\" && bootstrap_stage \"$2\" \"$3\" \"${@:4}\"", "bootstrap_stage", str(BOOTSTRAP_STAGE), str(ROOT), str(stage_out), *STRICT],
        capture=True,
    )
    boot_bin = Path(stage_result.stdout.strip().splitlines()[-1])
    run([str(boot_bin), str(source), str(boot_c)])
    workload_bin = OUT / "workload.bin"
    run(["gcc", *STRICT, str(boot_c), "-o", str(workload_bin)])

    output = subprocess.run([str(workload_bin)], check=True, text=True, capture_output=True).stdout
    if output != EXPECTED:
        raise SystemExit(f"Bootstrap output mismatch: expected {EXPECTED!r}, got {output!r}")

    boot_size = boot_c.stat().st_size
    if boot_size <= 65_536:
        raise SystemExit(f"Bootstrap-generated C did not cross former buffer threshold: size={boot_size}")

    print(f"PASS code_buffer_v3: {ASSIGNMENTS} assignments, Bootstrap output={ASSIGNMENTS}, C size={boot_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
