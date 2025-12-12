#!/usr/bin/env python3
import subprocess
import os
import sys
import platform
from pathlib import Path
import statistics as stats

NUM_RUNS = 100
PIN_CPU = "2" 

# Detect architecture to choose the correct Dynamic Linker
ARCH = platform.machine()
if ARCH == "x86_64":
    LD_NAME = "ld-linux-x86-64.so.2"
elif ARCH == "aarch64":
    LD_NAME = "ld-linux-aarch64.so.1"
else:
    print(f"[WARN] Unknown architecture {ARCH}, defaulting to x86_64 linker name.")
    LD_NAME = "ld-linux-x86-64.so.2"

# 1. Resolve paths relative to where this script lives
SCRIPT_DIR = Path(__file__).resolve().parent          # .../malloc-benchmarks/scripts
BENCH_DIR = SCRIPT_DIR.parent                         # .../malloc-benchmarks (Where binaries are)
PROJECT_ROOT = BENCH_DIR.parent                       # .../glibc-memory-profiler (Where install is)

# 2. Define library paths relative to the project root
LIB = PROJECT_ROOT / "glibc-install" / "lib"
LD = LIB / LD_NAME

LIB_BASELINE = PROJECT_ROOT / "orig-install" / "lib"
LD_BASELINE = LIB_BASELINE / LD_NAME

BENCHES = [
    "bench_fixed",
    "bench_var",
    "bench_mt",
    "bench_churn",
    "bench_churn_mt",
]

# Check if paths exist before starting
if not LD.exists():
    print(f"[ERROR] Custom loader not found at: {LD}")
    print(f"       Current script location: {SCRIPT_DIR}")
    print("       Double check your directory structure.")
    sys.exit(1)

# Helper to wrap command with taskset
def wrap_cmd(core, cmd_list):
    # Check if taskset exists (it should on Linux)
    return ["taskset", "-c", core] + cmd_list

# =================================================================================================
# MODES CONFIGURATION
# =================================================================================================
MODES = {
    # Upstream glibc
    "baseline": {
        "env": {},
        "cmd": lambda bench: wrap_cmd(PIN_CPU, [str(LD_BASELINE), "--library-path", str(LIB_BASELINE), f"bin/{bench}"]),
    },
    # Identical to baseline; verifies that the benchmark has no noise.
    "baseline2": {
        "env": {},
        "cmd": lambda bench: wrap_cmd(PIN_CPU, [str(LD_BASELINE), "--library-path", str(LIB_BASELINE), f"bin/{bench}"]),
    },
    # Custom glibc (profiler disabled).
    "off": {
        "env": {"GLIBC_MALLOC_PROFILE": "0"},
        "cmd": lambda bench: wrap_cmd(PIN_CPU, [str(LD), "--library-path", str(LIB), f"bin/{bench}"]),
    },
    # Custom glibc (profiler enabled).
    "on": {
        "env": {"GLIBC_MALLOC_PROFILE": "1", "GLIBC_MALLOC_PROFILE_BYTES": "262144"},
        "cmd": lambda bench: wrap_cmd(PIN_CPU, [str(LD), "--library-path", str(LIB), f"bin/{bench}"]),
    },
}
# =================================================================================================


def run_bench(bench: str, mode_name: str) -> float:
    """Run a single benchmark once in the given mode, return ns/op parsed."""
    mode = MODES[mode_name]
    env = os.environ.copy()
    env.update(mode["env"])
    
    cmd_list = mode["cmd"](bench)
    
    # cwd=BENCH_DIR ensures we run from `malloc-benchmarks/` 
    result = subprocess.run(
        cmd_list,
        env=env,
        cwd=BENCH_DIR,  
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
                # Parse "X ns per op"
                ns_value = float(line.split()[-1])
            except ValueError:
                pass

    if ns_value is None:
        print(f"[WARN] Could not parse ns per value for {bench} ({mode_name})")
        print("stdout:\n", result.stdout)
        raise RuntimeError("parse error")

    return ns_value


def get_benchmark_results():
    print(f"Running benchmarks with {NUM_RUNS} runs each (Pinned to CPU {PIN_CPU}).\n")
    results = {}  # (bench, mode) -> list[ns]
    for bench in BENCHES:
        for mode_name in MODES.keys():
            key = (bench, mode_name)
            results[key] = []
            print(f"== {bench} [{mode_name}] ==")
            for i in range(NUM_RUNS):
                try:
                    ns = run_bench(bench, mode_name)
                    results[key].append(ns)
                    print(f"  run {i + 1}/{NUM_RUNS}: {ns:.3f} ns")
                except RuntimeError:
                    sys.exit(1)
            print()
    return results


def print_summary(results):
    print(
        "\n======================= SUMMARY (average ns per operation) ================================\n"
    )
    print("NOTE: Value in parentheses is the ratio compared to `baseline`.\n")

    header = f"{'Benchmark':<18}"
    for mode in MODES:
        header += f"{mode:>20}"
    print(header)
    print("-" * len(header))

    for bench in BENCHES:
        try:
            baseline = stats.mean(results[(bench, "baseline")])
            row = f"{bench:<18}{baseline:20.3f}"
            for mode in MODES:
                if mode == "baseline":
                    continue
                value = stats.mean(results[(bench, mode)])
                ratio = value / baseline
                row += f"{value:8.3f} ({ratio:4.2f})".rjust(20)
            print(row)
        except KeyError:
            print(f"Skipping summary for {bench} due to missing data.")


def warm_up():
    print("Doing warmup runs...")
    try:
        # Check if the binary exists before warming up (Check inside bin/)
        bench_path = BENCH_DIR / "bin" / "bench_fixed"
        if not bench_path.exists():
            print(f"[WARN] Benchmark binary not found at {bench_path}")
            print("       Did you compile the benchmarks (e.g. `make`)?")
            return

        for i in range(10):
            run_bench("bench_fixed", "baseline")
    except Exception as e:
        print(f"[WARN] Warmup failed: {e}. Skipping...")


def main():
    warm_up()
    results = get_benchmark_results()
    print_summary(results)


if __name__ == "__main__":
    main()