# Verification

## Reference Interpreter

`ReferenceGPU` executes the GPU assembly program as scalar per-thread logic. It ignores cycle timing and produces final architectural memory/register state for comparison.

## Differential Testing

```bash
./build-ninja/simt_gpu --diff examples/vector_add.gpuasm
```

Diff mode compares final global memory and selected per-thread registers. Mismatch reports include thread, lane, warp, location, emulator value, reference value, kernel name, cycle count, and instruction counts.

## Invariant Checking

```bash
./build-ninja/simt_gpu --check-invariants examples/branch_divergence.gpuasm
```

The invariant checker validates active mask width, halted warp state, barrier registration, memory request bounds, scoreboard register ranges, and divergence stack sanity.

## Random Testing

```bash
python tools/random_gpu_program_generator.py --instructions 100 --seed 7 --output generated/random_gpu_7.gpuasm
python tools/run_random_gpu_diff_tests.py --count 20 --instructions 80
```

The random generator uses a conservative finite subset suitable for smoke-style differential testing.
