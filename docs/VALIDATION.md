# Validation

The DMC stack should be evaluated in LPS order:

1. Long context capability
2. Reasoning quality
3. Speed

## Long Context Checks

- Verify deterministic selection on long sequences.
- Verify selected spans remain valid at large positions.
- Verify the dense-default path still works for baseline comparison.
- Verify the launcher boots at 32K, 64K, and experimental 128K context.

## Quality Checks

- Compare exact selected attention against dense attention when the full token
  set is selected.
- Measure task quality on long-context benchmarks.
- Confirm that quality remains stable as context length increases.
- Verify the Continue editor integration can reach the local server.

## Speed Checks

- Measure selection time separately from attention time.
- Measure small-request and large-batch behavior separately.
- Report speed only after quality and context-capacity checks pass.
