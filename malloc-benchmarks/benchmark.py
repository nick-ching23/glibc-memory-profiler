#!/usr/bin/env python3
import subprocess
import os
from pathlib import Path
import statistics as stats

NUM_RUNS = 100

LIB = Path.cwd().parent / "glibc-install" / "lib"
LD = LIB / "ld-linux-x86-64.so.2"
LIB_BASELINE = Path.cwd().parent / "orig-install" / "lib"
LD_BASELINE = LIB_BASELINE / "ld-linux-x86-64.so.2"

BENCHES = [
    "bench_fixed",
    "bench_var",
    "bench_mt",
    "bench_churn",
    "bench_churn_mt",
]

MODES = {
    "baseline": {
        "env": {},
        "cmd": lambda bench: [LD_BASELINE, "--library-path", LIB_BASELINE, f"./{bench}"],
    },
    "off": {
        "env": {
            "GLIBC_MALLOC_PROFILE": "0",
        },
        "cmd": lambda bench: [LD, "--library-path", LIB, f"./{bench}"],
    },
    "on": {
        "env": {
            "GLIBC_MALLOC_PROFILE": "1",
            "GLIBC_MALLOC_PROFILE_BYTES": "262144",  # 256KB stride
        },
        "cmd": lambda bench: [LD, "--library-path", LIB, f"./{bench}"],
    },
}


def run_bench(bench: str, mode_name: str) -> float:
    """Run a single benchmark once in the given mode, return ns/op parsed."""
    mode = MODES[mode_name]
    env = os.environ.copy()
    env.update(mode["env"])
    cmd = mode["cmd"](bench)

    result = subprocess.run(
        cmd,
        env=env,
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        print(f"[ERROR] {bench} ({mode_name}) failed with code {result.returncode}")
        print("stdout:\n", result.stdout)
        print("stderr:\n", result.stderr)
        raise RuntimeError("benchmark failed")

    ns_value = None
    for line in result.stdout.splitlines():
        line = line.strip()
        if "ns per" in line:
            try:
                ns_value = float(line.split()[-1])
            except ValueError:
                pass

    if ns_value is None:
        print(f"[WARN] Could not parse ns per value for {bench} ({mode_name})")
        print("stdout:\n", result.stdout)
        raise RuntimeError("parse error")

    return ns_value


def main():
    # TODO: See if this is a good way to warm up.
    print("Doing warmup runs...")
    for i in range(50):
        run_bench("bench_fixed", "baseline")

    print(f"Running benchmarks with {NUM_RUNS} runs each.\n")
    results = {}  # (bench, mode) -> list[ns]
    for bench in BENCHES:
        for mode_name in MODES.keys():
            key = (bench, mode_name)
            results[key] = []
            print(f"== {bench} [{mode_name}] ==")
            for i in range(NUM_RUNS):
                ns = run_bench(bench, mode_name)
                results[key].append(ns)
                print(f"  run {i + 1}/{NUM_RUNS}: {ns:.3f} ns")
            print()

    print(
        "\n======================= SUMMARY (average ns per operation) ================================\n"
    )
    print(
        f"{'Benchmark':<18} "
        f"{'baseline':>12} "
        f"{'off':>12} "
        f"{'on':>12} "
        f"{'off/baseline':>16} "
        f"{'on/baseline':>16}"
    )
    print("-" * 91)

    for bench in BENCHES:
        baseline_mean = stats.mean(results[(bench, "baseline")])
        off_mean = stats.mean(results[(bench, "off")])
        on_mean = stats.mean(results[(bench, "on")])
        off_over_baseline = off_mean / baseline_mean
        on_over_baseline = on_mean / baseline_mean

        print(
            f"{bench:<18} "
            f"{baseline_mean:12.3f} "
            f"{off_mean:12.3f} "
            f"{on_mean:12.3f} "
            f"{off_over_baseline:16.2f} "
            f"{on_over_baseline:16.2f}"
        )


if __name__ == "__main__":
    main()
