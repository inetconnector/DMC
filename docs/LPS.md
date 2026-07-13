# LPS

LPS means:

- Long context first
- Quality second
- Speed third

## Priority Order

1. Hold much longer context than a dense baseline.
2. Keep reasoning quality high on long-context tasks.
3. Make the implementation fast without adding unnecessary complexity.

## Interpretation

- Long context is the primary product goal.
- Quality must not collapse just to win benchmarks.
- Speed matters, but it is a constraint on the design, not the only target.

## Success Signal

- The system stays deterministic.
- The system stays easy to validate.
- The system scales to large context lengths without ad hoc retrieval logic.
