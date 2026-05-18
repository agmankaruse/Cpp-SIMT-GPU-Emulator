#!/usr/bin/env python3
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import random_gpu_program_generator


ROOT = Path(__file__).resolve().parents[1]


def default_simulator():
    for directory in [ROOT / "build-ninja", ROOT / "build", ROOT / "build" / "Debug", ROOT / "build" / "Release"]:
        for name in ["simt_gpu.exe", "simt_gpu"]:
            candidate = directory / name
            if candidate.exists():
                return candidate
    return ROOT / "build-ninja" / "simt_gpu"


def main():
    parser = argparse.ArgumentParser(description="Run randomized GPU differential tests.")
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument("--instructions", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--sim", default=str(default_simulator()))
    args = parser.parse_args()

    generated = ROOT / "generated"
    failures = generated / "failures"
    generated.mkdir(exist_ok=True)
    failures.mkdir(parents=True, exist_ok=True)

    passed = 0
    failed = 0
    for index in range(args.count):
        seed = args.seed + index
        program = generated / f"random_gpu_{seed}.gpuasm"
        program.write_text(random_gpu_program_generator.generate(args.instructions, seed), encoding="utf-8")
        completed = subprocess.run(
            [args.sim, "--diff", str(program)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if completed.returncode == 0 and "PASS" in completed.stdout:
            passed += 1
        else:
            failed += 1
            shutil.copy2(program, failures / program.name)
            (failures / f"{program.stem}.log").write_text(completed.stdout, encoding="utf-8")
            print(f"FAIL seed={seed} saved={failures / program.name}")

    print(f"random_gpu_diff passed={passed} failed={failed} count={args.count}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
