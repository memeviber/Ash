from pathlib import Path
import statistics
import subprocess
import time

root = Path(__file__).resolve().parent
variants = {
    "c": root / "kernel_c.bin",
    "pyrel": root / "kernel_pyrel.bin",
    "nim": root / "kernel_nim",
    "cython": root / "kernel_cython.bin",
}

rows = []
for name, binary in variants.items():
    for sample in range(1, 10):
        start = time.perf_counter()
        for _ in range(10):
            completed = subprocess.run([str(binary)], stdout=subprocess.DEVNULL, check=True)
            if completed.returncode != 0:
                raise RuntimeError(f"{name} returned {completed.returncode}")
        elapsed = time.perf_counter() - start
        rows.append((name, sample, elapsed, elapsed / 10.0))

with (root / "timings.tsv").open("w", encoding="utf-8") as handle:
    handle.write("variant\tsample\ttotal_seconds\tper_run_seconds\n")
    for name, sample, total, per_run in rows:
        handle.write(f"{name}\t{sample}\t{total:.9f}\t{per_run:.9f}\n")

with (root / "timing_summary.tsv").open("w", encoding="utf-8") as handle:
    handle.write("variant\tmin_per_run\tmedian_per_run\tmean_per_run\n")
    for name in variants:
        values = [row[3] for row in rows if row[0] == name]
        handle.write(
            f"{name}\t{min(values):.9f}\t{statistics.median(values):.9f}\t{statistics.mean(values):.9f}\n"
        )

print((root / "timing_summary.tsv").read_text(encoding="utf-8"), end="")
