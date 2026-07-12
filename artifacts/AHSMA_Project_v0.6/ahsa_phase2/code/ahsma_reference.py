"""Reference implementation of speed-first block-sparse attention.

This module is intentionally simple and correct rather than production-optimized.
It demonstrates:
- block summaries
- local + global + retrieved selection
- query-dependent adaptive budgets
- exact attention over selected tokens

It is suitable for algorithmic experiments and teacher-trace generation.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional

import torch
import torch.nn.functional as F


@dataclass(frozen=True)
class AHSMAConfig:
    block_size: int = 64
    local_window: int = 2048
    global_tokens: int = 64
    route_dim: int = 32
    retrieved_blocks: int = 64
    min_retrieved_blocks: int = 16
    max_retrieved_blocks: int = 128
    adaptive_budget: bool = True
    uncertainty_margin: float = 0.05


@dataclass
class SelectionResult:
    token_indices: torch.Tensor
    block_indices: torch.Tensor
    selected_budget: int
    routing_margin: float


def _validate_qkv(q: torch.Tensor, k: torch.Tensor, v: torch.Tensor) -> None:
    if q.ndim != 2 or k.ndim != 2 or v.ndim != 2:
        raise ValueError("q, k and v must be rank-2 tensors [tokens, head_dim]")
    if k.shape != v.shape:
        raise ValueError("k and v must have identical shapes")
    if q.shape[-1] != k.shape[-1]:
        raise ValueError("q and k head dimensions must match")


def make_block_summaries(k: torch.Tensor, block_size: int) -> tuple[torch.Tensor, torch.Tensor]:
    """Return mean-pooled block summaries and valid lengths.

    Args:
        k: [n_tokens, head_dim]
        block_size: positive block size
    """
    if block_size <= 0:
        raise ValueError("block_size must be positive")
    n_tokens, d = k.shape
    n_blocks = (n_tokens + block_size - 1) // block_size
    padded = n_blocks * block_size
    if padded != n_tokens:
        k_pad = F.pad(k, (0, 0, 0, padded - n_tokens))
    else:
        k_pad = k
    blocks = k_pad.view(n_blocks, block_size, d)
    lengths = torch.full((n_blocks,), block_size, device=k.device, dtype=torch.long)
    lengths[-1] = n_tokens - (n_blocks - 1) * block_size
    mask = torch.arange(block_size, device=k.device)[None, :] < lengths[:, None]
    summaries = (blocks * mask[..., None]).sum(dim=1) / lengths[:, None]
    summaries = F.normalize(summaries, dim=-1)
    return summaries, lengths


def _adaptive_block_budget(scores: torch.Tensor, cfg: AHSMAConfig) -> tuple[int, float]:
    base = min(cfg.retrieved_blocks, scores.numel())
    if not cfg.adaptive_budget or scores.numel() < 2:
        return base, 1.0
    values, _ = torch.topk(scores, k=min(max(base, 2), scores.numel()))
    margin = float((values[0] - values[min(base - 1, values.numel() - 1)]).abs().item())
    # A low margin means the selector is uncertain, so expand the budget.
    if margin < cfg.uncertainty_margin:
        budget = min(cfg.max_retrieved_blocks, scores.numel())
    else:
        budget = max(cfg.min_retrieved_blocks, base)
    return budget, margin


def select_tokens(
    q_last: torch.Tensor,
    k: torch.Tensor,
    cfg: AHSMAConfig,
    route_q: Optional[torch.Tensor] = None,
    route_k: Optional[torch.Tensor] = None,
    global_positions: Optional[Iterable[int]] = None,
) -> SelectionResult:
    """Select local, global and semantically relevant historical tokens.

    q_last is one query vector [head_dim]. Optional route projections are matrices
    [route_dim, head_dim]. Without them, normalized head vectors are used directly.
    """
    if q_last.ndim != 1 or k.ndim != 2:
        raise ValueError("q_last must be [head_dim] and k must be [tokens, head_dim]")
    n_tokens = k.shape[0]
    summaries, _ = make_block_summaries(k, cfg.block_size)

    if route_q is not None:
        rq = F.normalize(route_q @ q_last, dim=-1)
    else:
        rq = F.normalize(q_last, dim=-1)
    if route_k is not None:
        rb = F.normalize(summaries @ route_k.T, dim=-1)
    else:
        rb = summaries
    scores = rb @ rq

    # Exclude blocks fully covered by the mandatory local window to avoid wasting retrieval budget.
    local_start = max(0, n_tokens - cfg.local_window)
    local_block_start = local_start // cfg.block_size
    eligible = torch.arange(scores.numel(), device=k.device) < local_block_start
    masked_scores = scores.masked_fill(~eligible, float("-inf"))
    eligible_count = int(eligible.sum().item())

    if eligible_count > 0:
        finite_scores = masked_scores[eligible]
        budget, margin = _adaptive_block_budget(finite_scores, cfg)
        budget = min(budget, eligible_count)
        selected_blocks = torch.topk(masked_scores, k=budget).indices
    else:
        budget, margin = 0, 1.0
        selected_blocks = torch.empty(0, dtype=torch.long, device=k.device)

    token_parts = [torch.arange(local_start, n_tokens, device=k.device)]
    if cfg.global_tokens > 0:
        token_parts.append(torch.arange(0, min(cfg.global_tokens, n_tokens), device=k.device))
    if global_positions is not None:
        gp = torch.as_tensor(list(global_positions), device=k.device, dtype=torch.long)
        gp = gp[(gp >= 0) & (gp < n_tokens)]
        token_parts.append(gp)
    if selected_blocks.numel() > 0:
        offsets = torch.arange(cfg.block_size, device=k.device)
        block_tokens = selected_blocks[:, None] * cfg.block_size + offsets[None, :]
        block_tokens = block_tokens.reshape(-1)
        block_tokens = block_tokens[block_tokens < n_tokens]
        token_parts.append(block_tokens)

    token_indices = torch.unique(torch.cat(token_parts), sorted=True)
    return SelectionResult(token_indices, selected_blocks.sort().values, budget, margin)


def exact_sparse_attention(
    q: torch.Tensor,
    k: torch.Tensor,
    v: torch.Tensor,
    token_indices: torch.Tensor,
) -> torch.Tensor:
    """Compute exact attention over selected tokens.

    q: [n_queries, head_dim], k/v: [n_tokens, head_dim]
    returns [n_queries, head_dim]
    """
    _validate_qkv(q, k, v)
    if token_indices.numel() == 0:
        raise ValueError("token_indices must not be empty")
    ks = k.index_select(0, token_indices)
    vs = v.index_select(0, token_indices)
    scores = q @ ks.T / (q.shape[-1] ** 0.5)
    probs = torch.softmax(scores.float(), dim=-1).to(q.dtype)
    return probs @ vs


def dense_attention(q: torch.Tensor, k: torch.Tensor, v: torch.Tensor) -> torch.Tensor:
    _validate_qkv(q, k, v)
    scores = q @ k.T / (q.shape[-1] ** 0.5)
    probs = torch.softmax(scores.float(), dim=-1).to(q.dtype)
    return probs @ v
