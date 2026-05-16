# Cpp-SIMT-GPU-Emulator

An educational C++17, cycle-based SIMT GPU emulator that models the execution ideas behind NVIDIA/AMD-style GPUs: streaming multiprocessors, resident warps, 32-lane SIMT execution, scoreboarding, functional-unit latency, global-memory latency hiding, coalesced memory transactions, shared-memory bank conflicts, and branch divergence/reconvergence.

This is not an RTL GPU and it is not intended to synthesize. It is a readable architecture simulator that runs small PTX-inspired `.gpuasm` programs and reports traces and performance statistics.

## Why This Is Technically Impressive

Many "tiny GPU" projects stop at running one vector instruction over a few lanes. This simulator goes further by modeling the architectural mechanisms that make real SIMT machines interesting:

- Multi-SM execution
- 32-lane warps by default
- Per-lane register and predicate state
- Scoreboarded issue
- Memory latency hiding across resident warps
- Coalesced global-memory transactions
- Branch divergence and reconvergence
- Per-SM shared memory with 32-bank conflict tracking
- Cycle-level statistics and trace output

## Architecture

```text
+------------------------------ GPU ------------------------------+
|                                                                 |
|  +---------------- SM 0 ----------------+  +---------------- SM 1 |
|  | Warp Scheduler                       |  | Warp Scheduler       |
|  | Scoreboards per warp                 |  | Scoreboards per warp |
|  |                                      |  |                      |
|  |  Warp 0: PC, active mask, lanes      |  |  Warp 0 ...          |
|  |  Warp 1: PC, active mask, lanes      |  |  Warp 1 ...          |
|  |  ...                                 |  |  ...                 |
|  |                                      |  |                      |
|  |  INT ALU pipes | SFU/MUL | LSU       |  |  INT | SFU | LSU     |
|  |  Shared memory, 32 banks             |  |  Shared memory       |
|  |  Memory request queue                |  |  Memory queue        |
|  +------------------+-------------------+  +----------+-----------+
|                     |                                 |
|                     +---------- Global Memory --------+
|                                Coalescer
+-----------------------------------------------------------------+
```

## SIMT Execution

The simulator issues one instruction for a warp. Every active lane in that warp executes the same instruction in parallel using its own register and predicate values. Inactive lanes are masked off and preserve their state.

## Warps And Lanes

A warp contains a configurable number of lanes, with 32 lanes by default. Each lane has its own integer register file and predicate register file. A warp owns the shared program counter, active mask, scoreboard, memory-wait state, and divergence stack.

Built-in values are exposed as instructions:

- `MOV.LANEID rD`
- `MOV.WARPID rD`
- `MOV.CTAID rD`
- `MOV.NTID rD`

## Active Masks

The active mask is a bit per lane. Arithmetic, predicate, and memory instructions only affect active lanes. This makes it possible for one warp to execute divergent paths without creating separate scalar threads in the simulator.

## Branch Divergence And Reconvergence

`BRA.P p0, label` evaluates `p0` per active lane. If all lanes agree, the warp simply branches or falls through. If some lanes take the branch and others do not, the simulator:

1. Creates a taken-lane mask and a fallthrough-lane mask.
2. Pushes the deferred path onto the divergence stack.
3. Executes the taken path first.
4. Reconverges when the current path halts or reaches a structured reconvergence point recorded by an unconditional `BRA`.

The statistics include divergence and reconvergence event counts.

## Warp Scheduling

Each SM owns a warp scheduler. The default policy is round-robin. A second policy, `oldest-ready`, chooses the ready warp that has gone the longest without issuing. The scheduler skips warps blocked by scoreboards, global-memory waits, busy functional units, or full memory queues.

## Scoreboarding

Each warp tracks pending register writes. Before issue, the scoreboard checks all source registers for the next instruction. If a source register is pending, that warp cannot issue. The scoreboard entry is cleared when the producing instruction writes back.

## Global Memory Latency Hiding

Global loads create memory requests with configurable latency, 100 cycles by default. The issuing warp waits for the load to return, but other ready resident warps on the same SM can continue issuing instructions. This is the core GPU latency-hiding behavior the simulator is designed to show.

## Memory Coalescing

For warp global memory operations, the memory coalescer groups active lane addresses into cache-line-style transactions. By default, the line size is 128 bytes. The simulator tracks:

- Warp memory instructions
- Coalesced transactions
- Active lane memory accesses
- Coalescing efficiency

Contiguous 32-lane, 4-byte accesses produce one 128-byte transaction. Strided accesses produce many transactions.

## Shared Memory And Bank Conflicts

Each SM has configurable shared memory, 32 KB by default. Shared memory is modeled as 32 banks. When active lanes touch addresses that map to the same bank, the simulator records bank conflicts and adds conflict cycles to the shared-memory latency.

## Supported Instruction Set

Control:

```text
NOP
HALT
BRA label
BRA.P p0, label
```

Integer arithmetic:

```text
MOV rD, rA
MOVI rD, imm
ADD rD, rA, rB
ADDI rD, rA, imm
SUB rD, rA, rB
MUL rD, rA, rB
MAD rD, rA, rB, rC
AND rD, rA, rB
OR rD, rA, rB
XOR rD, rA, rB
SHL rD, rA, imm
SHR rD, rA, imm
```

Predicates:

```text
SETP.EQ pD, rA, rB
SETP.NE pD, rA, rB
SETP.LT pD, rA, rB
SETP.GE pD, rA, rB
```

Memory:

```text
LD.GLOBAL rD, [rA + imm]
ST.GLOBAL [rA + imm], rB
LD.SHARED rD, [rA + imm]
ST.SHARED [rA + imm], rB
```

Built-ins:

```text
MOV.LANEID rD
MOV.WARPID rD
MOV.CTAID rD
MOV.NTID rD
```

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Run

Normal run:

```bash
./simt_gpu examples/vector_add.gpuasm
```

Trace run:

```bash
./simt_gpu --trace examples/vector_add.gpuasm
```

Configurable run:

```bash
./simt_gpu --sms 2 --warps-per-sm 8 --lanes 32 examples/vector_add.gpuasm
```

Scheduler selection:

```bash
./simt_gpu --scheduler oldest-ready examples/warp_latency_hiding.gpuasm
```

## Test

```bash
cd build
ctest --output-on-failure
```

The tests are assert-based and cover ALU execution, predicates, active masks, per-lane values, scoreboarding, memory latency hiding, global/shared memory, coalescing, branch divergence/reconvergence, and scheduler behavior.

## Example Trace Output

```text
[cycle     0] SM0 W0 PC=0 mask=11111111111111111111111111111111 :: MOV.LANEID r1
[cycle     1] SM0 W1 PC=0 mask=11111111111111111111111111111111 :: MOV.LANEID r1
[cycle    12] SM0 W0 PC=6 mask=11111111111111111111111111111111 :: LD.GLOBAL r7, [r6 + 0]
[cycle    13] SM0 W1 PC=6 mask=11111111111111111111111111111111 :: LD.GLOBAL r7, [r6 + 0]
```

## Example Statistics Output

```text
=== SIMT GPU Statistics ===
Total cycles: 139
Instructions issued: 160
Instructions retired: 160
IPC: 1.151
Warp occupancy: 100.000%
Number of memory requests: 48
Number of coalesced transactions: 48
Coalescing efficiency: 100.000%
Global memory stalls: 1408
Shared memory bank conflicts: 0
Branch divergence events: 0
Branch reconvergence events: 0
Scheduler idle cycles: 78
Scoreboard stall cycles: 32
```

## Default Configuration

```text
SMs: 2
Resident warps per SM: 8
Lanes per warp: 32
Registers per lane: 64
Predicate registers per lane: 4
Integer ALU pipelines per SM: 2
SFU / multiply pipelines per SM: 1
Load/store units per SM: 1
Shared memory per SM: 32 KB
Global memory: 1 MB
Global memory latency: 100 cycles
Shared memory latency: 4 cycles
Outstanding memory requests per SM: 16
```

## Program Format

Assembly files support `.kernel`, labels, blank lines, and comments using `#`, `//`, or `;`.

```text
.kernel vector_add
MOV.LANEID r1
MOV.WARPID r2
MOV.NTID r3
MUL r4, r2, r3
ADD r5, r4, r1
SHL r6, r5, 2
LD.GLOBAL r7, [r6 + 0]
LD.GLOBAL r8, [r6 + 4096]
ADD r9, r7, r8
ST.GLOBAL [r6 + 8192], r9
HALT
```

## Future Improvements

- Real PTX subset parser
- Cache hierarchy
- L1/L2 cache simulation
- Floating-point units
- Tensor-core-style matrix unit
- Occupancy calculator
- Web-based warp timeline viewer
- CUDA-like kernel launch API
- More advanced warp scheduling policies
- Better memory consistency model
