# Design Tradeoffs

- The simulator prioritizes readable architecture mechanisms over vendor-specific accuracy.
- The scalar reference model verifies final state and intentionally ignores timing.
- Shared memory bank conflicts affect timing and statistics, not logical values.
- The random generator uses conservative programs so randomized diff runs are stable.
- The static viewer avoids web dependencies and works from a local checkout.
