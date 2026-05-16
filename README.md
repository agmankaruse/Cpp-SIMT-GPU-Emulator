# Cpp-SIMT-GPU-Emulator

An educational C++17 SIMT GPU architecture simulator. It runs a readable PTX-inspired assembly format and models the execution mechanisms that make modern NVIDIA/AMD-style GPUs interesting: SMs, warps, active masks, branch divergence, scoreboarding, latency hiding, memory coalescing, shared memory banks, L1 cache behavior, barriers, warp-level primitives, occupancy, kernel launch indexing, timeline output, and benchmark-driven statistics.

This is not RTL and it is not a CUDA implementation. It is a cycle-oriented architecture model built to make GPU execution visible.

## Why This Is More Than A Toy GPU

This project intentionally goes beyond a tiny vector-lane interpreter:

- Multi-SM execution with configurable resident warps
- 32-lane SIMT warps by default
- Per-lane integer registers and predicate registers
- Scoreboarded issue with dependency stalls
- Global memory latency hiding across ready warps
- Cache-line-based memory coalescing with wasted-byte statistics
- Per-SM L1 data cache with hit/miss statistics
- Shared memory with 32-bank conflict analysis
- Branch divergence and reconvergence through active masks
- CTA-level `BAR.SYNC`
- Warp-level `VOTE`, `BALLOT`, and `SHFL` primitives
- CUDA-like `--grid` / `--block` launch indexing
- Occupancy reporting and timeline CSV output

## Architecture Diagram

```text
GPU
|-- SM 0
|   |-- Warp Scheduler
|   |-- Scoreboards
|   |-- Warp 0..N
|   |   |-- PC
|   |   |-- Active mask
|   |   |-- Divergence stack
|   |   `-- 32 lanes: registers + predicates
|   |-- ALU / SFU / LSU pipelines
|   |-- Shared Memory, 32 banks
|   `-- L1 Data Cache
|-- SM 1
|   `-- ...
`-- Global Memory
```

## SM Design

Each streaming multiprocessor owns resident warps, a scheduler, functional units, per-warp scoreboards, a shared-memory instance, an L1 data cache, a memory request queue, and CTA barrier state. Every cycle, an SM completes writebacks, retires memory requests, checks barriers and reconvergence, finds eligible warps, and issues one ready warp instruction.

## Warp And Lane Model

A warp has one program counter and one active mask. Every lane has its own integer register file and predicate file. Built-in instructions expose lane, warp, CTA, thread, and launch dimensions:

```text
MOV.LANEID rD
MOV.WARPID rD
MOV.TID rD
MOV.CTAID rD
MOV.GTID rD
MOV.NTID rD
MOV.NCTA rD
```

## SIMT Active Masks

The active mask controls which lanes execute an instruction. Inactive lanes are skipped and preserve their state. Divergent branches split masks, and reconvergence restores deferred lanes.

## Branch Divergence And Reconvergence

`BRA.P p0, label` evaluates `p0` per lane. If all active lanes agree, the warp branches normally. If lanes disagree, the simulator pushes the fallthrough path on a divergence stack, executes the taken mask, then reconverges at structured branch or halt points. Stats track divergence and reconvergence events.

## Warp Scheduling Policies

Supported scheduler policies:

```text
round_robin
greedy_then_oldest
oldest_ready
two_level
```

The scheduler skips warps stalled by scoreboards, memory, barriers, functional-unit availability, or full memory queues. Stats report idle cycles, issue efficiency, average eligible warps per cycle, and selected-warp distribution.

## Scoreboarding

Every warp tracks pending destination registers. An instruction cannot issue until all source registers are ready. Writeback clears scoreboard entries and increments retired instruction counts.

## Memory Coalescing

For each warp global memory instruction, active lane addresses are grouped by cache line. The simulator reports requested bytes, transferred bytes, wasted bytes, coalesced transactions, coalescing efficiency, and transactions per warp memory instruction.

Contiguous lane accesses should produce one 128-byte transaction. Strided lane accesses can produce one transaction per lane.

## L1 Cache Model

Each SM has a configurable L1 data cache:

- Cache size
- Cache line size
- Associativity
- Hit latency
- Miss latency

Global memory instructions access the L1 per coalesced cache line. The simulator tracks L1 hits, misses, hit rate, and cache miss stall cycles.

## Shared Memory And Bank Conflicts

Shared memory is per-SM and split into 32 banks. For each shared-memory instruction, active lane addresses are mapped to banks. Multiple different addresses in the same bank create conflicts and extra latency. Same-address shared-load broadcasts are treated as conflict-free.

Stats include shared-memory accesses, conflict events, total conflicts, max conflict degree, and average conflict degree.

## Barrier Synchronization

`BAR.SYNC` waits until all active warps in the same CTA reach the barrier. The simulator releases them together and tracks barrier count and stall cycles.

## Warp-Level Primitives

The simulator includes simple warp communication instructions:

```text
VOTE.ALL pD, pA
VOTE.ANY pD, pA
BALLOT rD, pA
SHFL.IDX rD, rA, rB
```

These are intentionally educational rather than CUDA-exact. They demonstrate warp collectives and lane-to-lane register exchange.

## Occupancy Calculator

Occupancy mode estimates theoretical active warps per SM from register use, shared memory, block size, and warp slots:

```bash
./simt_gpu --occupancy examples/vector_add.gpuasm
```

The report prints registers per thread, shared memory per CTA, warps per CTA, theoretical active warps per SM, occupancy percentage, and limiting factor.

## Kernel Launch Model

Use CUDA-like launch dimensions:

```bash
./simt_gpu --grid 4 --block 128 examples/vector_add.gpuasm
```

`grid` is the number of CTAs. `block` is threads per CTA. Threads are grouped into warps and CTAs are mapped across SMs.

## Config Files

Architecture files live under `configs/`:

```text
configs/small_debug.gpuconf
configs/default_gpu.gpuconf
configs/memory_stress.gpuconf
configs/high_occupancy.gpuconf
```

Run with:

```bash
./simt_gpu --config configs/default_gpu.gpuconf examples/vector_add.gpuasm
```

Config keys include SM count, warps per SM, lanes per warp, register count, shared/global memory sizes, ALU/SFU/LSU counts, memory latencies, outstanding requests, scheduler policy, cache size, cache line size, cache associativity, and launch dimensions.

## Timeline CSV Output

Generate a timeline:

```bash
./simt_gpu --timeline outputs/gpu_timeline.csv examples/vector_add.gpuasm
```

CSV columns:

```text
cycle,sm,warp,instruction,pc,event,active_mask,stall_reason
```

Events include `ISSUE`, `EXECUTE`, `MEMORY_REQUEST`, `MEMORY_RETURN`, `WRITEBACK`, `DIVERGE`, `RECONVERGE`, `BARRIER_WAIT`, `BARRIER_RELEASE`, and `HALT`.

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Run

```bash
./simt_gpu examples/vector_add.gpuasm
./simt_gpu --trace examples/vector_add.gpuasm
./simt_gpu --scheduler oldest_ready benchmarks/scheduler_latency_hiding.gpuasm
./simt_gpu --config configs/memory_stress.gpuconf examples/coalescing_good_vs_bad.gpuasm
```

## Test

```bash
cd build
ctest --output-on-failure
```

The tests cover ALU execution, predicates, active masks, scoreboard stalls, memory latency hiding, global/shared memory, coalescing, branch divergence, scheduler policies, config loading, L1 cache behavior, warp primitives, barriers, occupancy, launch indexing, timeline output, and shared-memory conflict modeling.

## Benchmark Suite

```text
benchmarks/vector_add.gpuasm
benchmarks/saxpy.gpuasm
benchmarks/memory_coalescing_contiguous.gpuasm
benchmarks/memory_coalescing_strided.gpuasm
benchmarks/branch_divergence_heavy.gpuasm
benchmarks/shared_memory_bank_conflict.gpuasm
benchmarks/shared_memory_no_conflict.gpuasm
benchmarks/warp_shuffle_reduce.gpuasm
benchmarks/barrier_sync.gpuasm
benchmarks/latency_hiding_many_warps.gpuasm
benchmarks/scheduler_latency_hiding.gpuasm
```

Example:

```bash
./simt_gpu --config configs/high_occupancy.gpuconf benchmarks/latency_hiding_many_warps.gpuasm
```

## Example Statistics Output

```text
=== SIMT GPU Statistics ===
Total cycles: 144
Instructions issued: 160
Instructions retired: 160
IPC: 1.111
Warp occupancy: 100.000%
Number of memory requests: 48
Number of coalesced transactions: 48
Requested bytes: 6144
Transferred bytes: 6144
Wasted bytes: 0
Coalescing efficiency: 100.000%
L1 hits: 24
L1 misses: 24
L1 hit rate: 50.000%
Shared memory bank conflict events: 0
Branch divergence events: 0
Barrier count: 0
Average eligible warps per cycle: 5.421
```

## Future Work

- Real PTX subset parser
- L2 cache and cache coherence experiments
- More accurate memory consistency model
- Floating-point and SFU instruction families
- Tensor-core-style matrix unit
- Occupancy calculator with launch-resource annotations
- Web-based warp timeline viewer
- CUDA-like host launch API
- More advanced warp scheduling policies
- Instruction-level replay for memory divergence
