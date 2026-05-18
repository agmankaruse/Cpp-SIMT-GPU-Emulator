# Examples

```bash
./build-ninja/simt_gpu examples/vector_add.gpuasm
./build-ninja/simt_gpu --trace examples/branch_divergence.gpuasm
./build-ninja/simt_gpu --diff examples/vector_add.gpuasm
./build-ninja/simt_gpu --timeline outputs/gpu_warp_timeline.csv examples/vector_add.gpuasm
```

Representative generated outputs are under `results/example_run/`.

## Sample Output

Normal run:

```text
=== SIMT GPU Statistics ===
Total cycles: 332
Instructions issued: 112
IPC: 0.337
```

Diff run:

```text
PASS diff kernel=vector_add cycles=332 emulator_instructions=112 reference_instructions=3584
```

Timeline CSV:

```text
cycle,sm,cta,warp,pc,instruction,event,active_mask,eligible,selected,stall_reason,memory_transaction_count,cache_result,divergence_depth
0,0,0,0,0,"MOV.GTID r5",ISSUE,11111111111111111111111111111111,1,1,"",,,
```
