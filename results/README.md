# Results

`results/example_run/` contains real generated outputs from the current build, including normal run, trace run, diff run, benchmark run, random diff summary, and timeline CSVs for representative kernels.

Regenerate after behavior changes:

```bash
python tools/run_gpu_benchmarks.py
python tools/run_random_gpu_diff_tests.py --count 20 --instructions 80
./build-ninja/simt_gpu --timeline outputs/gpu_warp_timeline.csv examples/vector_add.gpuasm
```

## Current Sample Outputs

Benchmark run:

```text
PASS memory_coalescing_contiguous cycles=224 ipc=0.357
PASS memory_coalescing_strided cycles=224 ipc=0.357
PASS branch_divergence_heavy cycles=149 ipc=1.289
PASS vector_add cycles=332 ipc=0.337
```

Randomized diff run:

```text
random_gpu_diff passed=20 failed=0 count=20
```
