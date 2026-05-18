# Contributing

## Local Workflow

```bash
cmake -S . -B build-ninja -G Ninja -DENABLE_TESTS=ON -DENABLE_WARNINGS=ON
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

## Guidelines

- Keep simulator behavior changes focused.
- Add tests for correctness-sensitive changes.
- Update docs for CLI, architecture, tooling, or benchmark changes.
- Avoid new external dependencies unless the benefit is clear.
