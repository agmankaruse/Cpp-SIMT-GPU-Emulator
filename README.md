# C++ SIMT GPU Emulator

A C++17 SIMT GPU architecture emulator modeling streaming multiprocessors, 32-lane-style warps, active masks, per-lane registers, scoreboarding, warp scheduling, memory latency hiding, global memory, L1 cache behavior, coalescing, shared memory bank conflicts, barriers, branch divergence/reconvergence, occupancy, benchmark analysis, and differential verification against a scalar reference interpreter.

## Architecture

```text
Grid / CTAs
    |
    v
Streaming Multiprocessors
    |
    +-- Warp scheduler -> scoreboard -> INT/SFU/LSU pipelines
    |
    +-- Warps -> lanes -> per-lane registers/predicates/active masks
    |
    +-- L1 cache / coalescer / memory queue
    |
    +-- Shared memory banks / barriers / divergence stack
```

## Key Features

- Multi-SM launch model with configurable CTAs and block size
- SIMT warps with per-lane registers, predicates, and active masks
- Scoreboarded warp issue and selectable scheduling policies
- Branch divergence/reconvergence stack
- Global memory latency, L1 cache, and memory coalescing model
- Shared memory with bank conflict timing
- Barrier synchronization
- Occupancy report with limiting-resource guidance
- Warp timeline CSV generation and local static viewer
- Scalar reference interpreter and `--diff` mode
- Invariant checker and debug dumps
- Randomized differential testing and benchmark automation

## Build

```bash
cmake -S . -B build-ninja -G Ninja -DENABLE_TESTS=ON -DENABLE_WARNINGS=ON
cmake --build build-ninja
```

Useful CMake options:

- `ENABLE_WARNINGS=ON`
- `ENABLE_ASAN=ON` on non-MSVC compilers
- `ENABLE_TRACE=ON`
- `ENABLE_TESTS=ON`

## Run

```bash
./build-ninja/simt_gpu examples/vector_add.gpuasm
./build-ninja/simt_gpu --trace examples/branch_divergence.gpuasm
./build-ninja/simt_gpu --timeline outputs/gpu_warp_timeline.csv examples/vector_add.gpuasm
./build-ninja/simt_gpu --occupancy --grid 8 --block 128 examples/vector_add.gpuasm
./build-ninja/simt_gpu --version
```

## Verification

```bash
./build-ninja/simt_gpu --diff examples/vector_add.gpuasm
./build-ninja/simt_gpu --check-invariants examples/branch_divergence.gpuasm
./build-ninja/simt_gpu --dump-on-fail --diff examples/vector_add.gpuasm
python tools/run_random_gpu_diff_tests.py --count 20 --instructions 80
```

Diff mode compares final global memory and selected per-thread registers against a scalar reference interpreter.

## Tests

```bash
ctest --test-dir build-ninja --output-on-failure
```

The suite covers ALU execution, warp execution, scoreboarding, global/shared memory, memory coalescing, divergence, scheduling, cache behavior, occupancy, barriers, timeline output, the reference interpreter, diff mode, invariant checking, and random-program smoke coverage.

## Benchmarks And Analysis

```bash
python tools/run_gpu_benchmarks.py
python tools/analyze_coalescing.py
python tools/compare_schedulers.py
python tools/plot_gpu_results.py
```

Outputs are written to `outputs/`, including `outputs/gpu_benchmark_results.csv`, coalescing reports, scheduler comparison reports, plots, and timeline CSVs.

## Visualization

Open `viewer/gpu_warp_timeline_viewer.html` in a browser and select a generated CSV such as `outputs/gpu_warp_timeline.csv` or a benchmark timeline under `outputs/timelines/`. The viewer works locally with no external dependencies.

## Project Structure

- `include/`, `src/`: emulator core
- `tests/`: CTest programs
- `examples/`: runnable kernels
- `benchmarks/`: analysis kernels
- `tools/`: random testing, benchmarks, coalescing, scheduler, plotting
- `viewer/`: static warp timeline viewer
- `docs/`: architecture, verification, benchmark, diagrams, limitations
- `results/`: generated example outputs

## Limitations

This is an educational architecture emulator, not a CUDA runtime or binary-compatible GPU. The ISA is a compact GPU assembly dialect, the reference interpreter focuses on architectural final state, and the timing model is intentionally readable rather than vendor-accurate.

## Future Work

- Add richer predication and memory consistency tests
- Improve scalar reference handling for complex shared-memory cross-thread idioms
- Add JSON trace export
- Add performance regression thresholds
- Model additional cache policies and scheduler heuristics

## Resume / Interview Talking Points

- Built a C++17 SIMT GPU emulator modeling multi-SM execution, 32-lane warps, scoreboarding, branch divergence/reconvergence, memory coalescing, shared memory bank conflicts, and latency hiding.
- Developed benchmark, visualization, and verification tooling for analyzing warp scheduling, occupancy, memory behavior, and SIMT execution efficiency.

## What I Learned

This project reinforced SIMT execution, warp scheduling, memory coalescing, branch divergence and reconvergence, shared memory bank conflicts, occupancy limits, and how GPUs hide memory latency with many eligible warps.
