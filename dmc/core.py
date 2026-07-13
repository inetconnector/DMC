"""Deterministic multiresolution context reference."""
from __future__ import annotations

from dataclasses import dataclass
from bisect import bisect_left, bisect_right
from collections import defaultdict
from typing import Iterable, Sequence

import torch


@dataclass(frozen=True)
class DMCConfig:
    block_size: int = 64
    local_window: int = 2048
    global_tokens: int = 64
    replay_levels: int = 4


@dataclass(frozen=True)
class TokenRef:
    physical_cell: int
    logical_pos: int
    seq_id: int


@dataclass(frozen=True)
class Span:
    seq_id: int
    level: int
    first_pos: int
    last_pos: int
    token_ids: tuple[int, ...]


@dataclass(frozen=True)
class DMCSelection:
    token_ids: torch.Tensor
    spans: tuple[Span, ...]


def _validate_qkv(q: torch.Tensor, k: torch.Tensor, v: torch.Tensor) -> None:
    if q.ndim != 2 or k.ndim != 2 or v.ndim != 2:
        raise ValueError("q, k and v must be rank-2 tensors [tokens, head_dim]")
    if k.shape != v.shape:
        raise ValueError("k and v must have identical shapes")
    if q.shape[-1] != k.shape[-1]:
        raise ValueError("q and k head dimensions must match")


def _unique_preserve_order(values: Iterable[int]) -> tuple[int, ...]:
    seen: set[int] = set()
    out: list[int] = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        out.append(value)
    return tuple(out)


def dense_attention(q: torch.Tensor, k: torch.Tensor, v: torch.Tensor) -> torch.Tensor:
    _validate_qkv(q, k, v)
    scores = q @ k.T / (q.shape[-1] ** 0.5)
    probs = torch.softmax(scores.float(), dim=-1).to(q.dtype)
    return probs @ v


def exact_attention(
    q: torch.Tensor,
    k: torch.Tensor,
    v: torch.Tensor,
    token_ids: Sequence[int] | torch.Tensor,
) -> torch.Tensor:
    _validate_qkv(q, k, v)
    ids = torch.as_tensor(token_ids, dtype=torch.long, device=k.device)
    if ids.numel() == 0:
        raise ValueError("token_ids must not be empty")
    if int(ids.min().item()) < 0 or int(ids.max().item()) >= k.shape[0]:
        raise IndexError("token id out of range")
    ks = k.index_select(0, ids)
    vs = v.index_select(0, ids)
    scores = q @ ks.T / (q.shape[-1] ** 0.5)
    probs = torch.softmax(scores.float(), dim=-1).to(q.dtype)
    return probs @ vs


class DMCIndex:
    """Deterministic span selection over a logical KV sequence."""

    def __init__(self, cfg: DMCConfig):
        if cfg.block_size <= 0:
            raise ValueError("block_size must be positive")
        if cfg.local_window < 0 or cfg.global_tokens < 0 or cfg.replay_levels < 0:
            raise ValueError("configuration values must not be negative")
        self.cfg = cfg
        self._refs: list[TokenRef] = []
        self._refs_by_seq: dict[int, list[TokenRef]] = {}

    @property
    def refs(self) -> tuple[TokenRef, ...]:
        return tuple(self._refs)

    def rebuild(self, refs: Sequence[TokenRef]) -> None:
        self._refs = sorted(refs, key=lambda r: (r.seq_id, r.logical_pos, r.physical_cell))
        refs_by_seq: dict[int, list[TokenRef]] = defaultdict(list)
        for ref in self._refs:
            refs_by_seq[ref.seq_id].append(ref)
        self._refs_by_seq = dict(refs_by_seq)

    def clear(self) -> None:
        self._refs.clear()
        self._refs_by_seq.clear()

    def append(self, ref: TokenRef) -> None:
        self._refs.append(ref)
        self._refs.sort(key=lambda r: (r.seq_id, r.logical_pos, r.physical_cell))
        refs = self._refs_by_seq.setdefault(ref.seq_id, [])
        refs.append(ref)
        refs.sort(key=lambda r: (r.logical_pos, r.physical_cell))

    def select(self, seq_id: int, current_pos: int) -> DMCSelection:
        refs = self._refs_by_seq.get(seq_id, [])
        refs = refs[: bisect_right(refs, current_pos, key=lambda r: r.logical_pos)]
        if not refs:
            raise RuntimeError("sequence is empty")

        spans: list[Span] = []
        local_start = max(0, current_pos - self.cfg.local_window + 1)

        spans.append(self._build_span(seq_id, refs, local_start, current_pos, level=0))

        if self.cfg.global_tokens > 0:
            global_end = min(self.cfg.global_tokens - 1, current_pos)
            if global_end >= 0:
                spans.append(self._build_span(seq_id, refs, 0, global_end, level=0))

        history_end = local_start - 1
        for level in range(self.cfg.replay_levels):
            if history_end < 0:
                break
            span_size = self.cfg.block_size << level
            end = ((history_end + 1) // span_size) * span_size - 1
            if end < 0:
                continue
            start = max(0, end - span_size + 1)
            if start >= local_start:
                continue
            spans.append(self._build_span(seq_id, refs, start, end, level=level + 1))

        token_ids = _unique_preserve_order(
            token for span in spans for token in span.token_ids
        )
        return DMCSelection(torch.tensor(token_ids, dtype=torch.long), tuple(spans))

    def _build_span(
        self,
        seq_id: int,
        refs: Sequence[TokenRef],
        start_pos: int,
        end_pos: int,
        *,
        level: int,
    ) -> Span:
        if start_pos > end_pos:
            raise ValueError("invalid span bounds")
        left = bisect_left(refs, start_pos, key=lambda r: r.logical_pos)
        right = bisect_right(refs, end_pos, key=lambda r: r.logical_pos)
        selected = refs[left:right]
        token_ids = tuple(
            ref.physical_cell
            for ref in selected
        )
        if not token_ids:
            raise RuntimeError("selected span is empty")
        return Span(
            seq_id=seq_id,
            level=level,
            first_pos=start_pos,
            last_pos=end_pos,
            token_ids=token_ids,
        )


def exact_attention_from_selection(
    q: torch.Tensor,
    k: torch.Tensor,
    v: torch.Tensor,
    selection: DMCSelection,
) -> torch.Tensor:
    return exact_attention(q, k, v, selection.token_ids)
