# GPU Architecture

The emulator models a configurable SIMT GPU with streaming multiprocessors, resident warps, per-lane registers and predicates, active masks, a scoreboard, warp scheduler, functional units, global memory, L1 cache, memory coalescer, shared memory, barriers, and branch divergence/reconvergence.

Each cycle, an SM completes writes and memory requests, evaluates warp readiness, selects one eligible warp through the configured scheduler, and issues the current instruction if its operands and functional unit are available.

## Core Concepts

- Active mask: determines which lanes participate in each warp instruction.
- Scoreboard: prevents a warp from issuing with unresolved source registers.
- Divergence stack: stores deferred masks and PCs for divergent branches.
- Coalescer: groups active lane memory addresses into memory transactions.
- Shared memory banks: model conflict penalties without changing correctness.
- Occupancy: estimates resident CTAs/warps and limiting resources.
