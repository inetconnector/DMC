"""Microbenchmark and numerical comparison for the AHSMA reference selector."""
from __future__ import annotations

import argparse
import json
import time

import torch
import torch.nn.functional as F

from ahsma_reference import AHSMAConfig, dense_attention, exact_sparse_attention, select_tokens


def synchronize(device: torch.device) -> None:
    if device.type == "cuda":
        torch.cuda.synchronize(device)


def timed(fn, repeats: int, device: torch.device) -> tuple[float, object]:
    for _ in range(3):
        out = fn()
    synchronize(device)
    start = time.perf_counter()
    for _ in range(repeats):
        out = fn()
    synchronize(device)
    return (time.perf_counter() - start) / repeats, out


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--tokens", type=int, default=32768)
    p.add_argument("--head-dim", type=int, default=128)
    p.add_argument("--queries", type=int, default=1)
    p.add_argument("--block-size", type=int, default=64)
    p.add_argument("--local", type=int, default=2048)
    p.add_argument("--retrieved-blocks", type=int, default=64)
    p.add_argument("--repeats", type=int, default=10)
    p.add_argument("--device", choices=["cpu", "cuda"], default="cpu")
    p.add_argument("--dtype", choices=["float32", "float16", "bfloat16"], default="float32")
    args = p.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise SystemExit("CUDA requested but unavailable")
    device = torch.device(args.device)
    dtype = getattr(torch, args.dtype)
    if device.type == "cpu" and dtype == torch.float16:
        raise SystemExit("float16 CPU benchmark is not supported reliably; use float32/bfloat16")

    torch.manual_seed(7)
    q = torch.randn(args.queries, args.head_dim, device=device, dtype=dtype)
    k = torch.randn(args.tokens, args.head_dim, device=device, dtype=dtype)
    v = torch.randn_like(k)
    cfg = AHSMAConfig(
        block_size=args.block_size,
        local_window=args.local,
        retrieved_blocks=args.retrieved_blocks,
        min_retrieved_blocks=args.retrieved_blocks,
        max_retrieved_blocks=args.retrieved_blocks,
        adaptive_budget=False,
    )

    selection_time, selection = timed(lambda: select_tokens(q[-1], k, cfg), args.repeats, device)
    dense_time, dense_out = timed(lambda: dense_attention(q, k, v), args.repeats, device)
    sparse_time, sparse_out = timed(
        lambda: exact_sparse_attention(q, k, v, selection.token_indices), args.repeats, device
    )

    cosine = float(F.cosine_similarity(dense_out.float().flatten(), sparse_out.float().flatten(), dim=0).item())
    active = int(selection.token_indices.numel())
    result = {
        "device": str(device),
        "dtype": args.dtype,
        "tokens": args.tokens,
        "active_tokens": active,
        "active_fraction": active / args.tokens,
        "selection_ms": selection_time * 1000,
        "dense_attention_ms": dense_time * 1000,
        "sparse_attention_ms": sparse_time * 1000,
        "reference_total_sparse_ms": (selection_time + sparse_time) * 1000,
        "attention_only_speedup": dense_time / sparse_time,
        "reference_total_speedup": dense_time / (selection_time + sparse_time),
        "output_cosine_similarity": cosine,
        "note": "Reference implementation materializes summaries and selections each call; optimized kernels should cache summaries and fuse routing/attention.",
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
