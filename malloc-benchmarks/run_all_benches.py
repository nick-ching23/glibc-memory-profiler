#!/usr/bin/env python3
import subprocess
import os
from pathlib import Path
import statistics as stats

# -------------------------
# Config
# -------------------------

NUM_RUNS = 100

BASE_DIR = Path.home() / "Desktop" / "malloc-benchmarks"
LOADER = Path.home() / "Desktop" / "glibc-install" / "lib" / "ld-linux-aarch64.so.1"
LIB_PATH = Path.home() / "Desktop" / "glibc-install" / "lib"

BENCHES = [
    "bench_fixed",
    "bench_var",
    "bench_mt",
    "bench_churn",
    "bench_churn_mt",
]

MODES = {
    "system": {
        "label": "System glibc",
        "env": {},
        "cmd": lambda bench: [f"./{bench}"],
    },
    "custom_off": {
        "label": "Custom (prof OFF)",
        "env": {
            "GLIBC_MALLOC_PROFILE": "0",
        },
        "cmd": lambda bench: [
            str(LOADER),
            "--library-path",
            str(LIB_PATH),
            f"./{bench}",
        ],
    },
    "custom_on": {
        "label": "Custom (prof ON)",
        "env": {
            "GLIBC_MALLOC_PROFILE": "1",
            "GLIBC_MALLOC_PROFILE_BYTES": "262144",  # 256KB stride
        },
        "cmd": lambda bench: [
            str(LOADER),
            "--library-path",
            str(LIB_PATH),
            f"./{bench}",
        ],
    },
}


# -------------------------
# Helpers
# -------------------------

def run_bench(bench: str, mode_name: str) -> float:
    """Run a single benchmark once in the given mode, return ns/op parsed."""
    mode = MODES[mode_name]
    env = os.environ.copy()
    env.update(mode["env"])
    cmd = mode["cmd"](bench)

    result = subprocess.run(
        cmd,
        cwd=BASE_DIR,
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


def pct_overhead(base: float, test: float) -> float:
    """Return percentage overhead of 'test' vs 'base'."""
    if base <= 0:
        return 0.0
    return (test - base) / base * 100.0


def main():
    results = {}  # (bench, mode) -> list[ns]

    print(f"Running benchmarks in {BASE_DIR} with {NUM_RUNS} runs each.\n")

    for bench in BENCHES:
        for mode_name in MODES.keys():
            key = (bench, mode_name)
            results[key] = []
            print(f"== {bench} [{MODES[mode_name]['label']}] ==")
            for i in range(NUM_RUNS):
                ns = run_bench(bench, mode_name)
                results[key].append(ns)
                print(f"  run {i+1}/{NUM_RUNS}: {ns:.3f} ns")
            print()

    # Summary
    print("\n==================== SUMMARY (ns per op, means) ====================\n")
    print(
        f"{'Benchmark':<18} "
        f"{'system':>10} "
        f"{'cust_off':>10} "
        f"{'cust_on':>10} "
        f"{'ON vs sys%':>12} "
        f"{'ON vs off%':>12}"
    )
    print("-" * 74)

    for bench in BENCHES:
        sys_mean = stats.mean(results[(bench, "system")])
        off_mean = stats.mean(results[(bench, "custom_off")])
        on_mean  = stats.mean(results[(bench, "custom_on")])

        on_vs_sys = pct_overhead(sys_mean, on_mean)
        on_vs_off = pct_overhead(off_mean, on_mean)

        print(
            f"{bench:<18} "
            f"{sys_mean:10.3f} "
            f"{off_mean:10.3f} "
            f"{on_mean:10.3f} "
            f"{on_vs_sys:12.2f} "
            f"{on_vs_off:12.2f}"
        )

    print("\nNotes:")
    print("  - system     = default system glibc")
    print("  - cust_off   = custom glibc, profiler runtime OFF")
    print("  - cust_on    = custom glibc, profiler runtime ON")
    print("  - ON vs sys% = (cust_on - system) / system * 100")
    print("  - ON vs off% = (cust_on - cust_off) / cust_off * 100\n")


if __name__ == "__main__":
    main()