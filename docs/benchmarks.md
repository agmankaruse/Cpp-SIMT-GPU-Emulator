# Benchmarks

Run:

```bash
python tools/run_gpu_benchmarks.py
python tools/analyze_coalescing.py
python tools/compare_schedulers.py
python tools/plot_gpu_results.py
```

## vector_add

Demonstrates global memory loads, arithmetic, stores, and coalescing.

## memory_coalescing_contiguous

Shows efficient lane-contiguous memory access. A good result has high coalescing efficiency.

## memory_coalescing_strided

Shows less efficient access patterns and extra memory transactions.

## branch_divergence_heavy

Demonstrates active masks, divergence events, and reconvergence.

## barrier_sync

Exercises barriers and shared memory synchronization.

## shared_memory_bank_conflict / shared_memory_no_conflict

Contrasts bank conflict timing against correctness-preserving shared memory behavior.

## scheduler_latency_hiding / latency_hiding_many_warps

Shows how additional eligible warps hide memory latency and how scheduling policy affects idle cycles.
