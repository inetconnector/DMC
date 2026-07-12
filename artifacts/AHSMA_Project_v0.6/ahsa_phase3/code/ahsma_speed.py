"""Speed-first AHSMA research prototype.

This module separates one-time/incremental index maintenance from the per-token
critical path. It remains a PyTorch reference, not a production fused kernel.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional
import torch
import torch.nn.functional as F


@dataclass(frozen=True)
class SpeedConfig:
    block_size: int = 64
    local_window: int = 2048
    global_tokens: int = 64
    retrieved_blocks: int = 64
    route_refresh: int = 8
    route_dim: int = 32


@dataclass
class Route:
    token_indices: torch.Tensor
    block_indices: torch.Tensor
    reused: bool = False


class PersistentBlockIndex:
    """Persistent mean-pooled block index for one KV layer/head group."""

    def __init__(self, cfg: SpeedConfig, device: torch.device | str):
        self.cfg = cfg
        self.device = torch.device(device)
        self._k: Optional[torch.Tensor] = None
        self._summaries: Optional[torch.Tensor] = None
        self._indexed_tokens = 0
        self._cached_route: Optional[Route] = None
        self._cached_step = -1

    @property
    def summaries(self) -> torch.Tensor:
        if self._summaries is None:
            raise RuntimeError("index is empty")
        return self._summaries

    def build(self, k: torch.Tensor) -> None:
        if k.ndim != 2:
            raise ValueError("k must be [tokens, head_dim]")
        self._k = k
        self._summaries = self._pool_all(k)
        self._indexed_tokens = k.shape[0]
        self._cached_route = None
        self._cached_step = -1

    def update(self, k: torch.Tensor) -> None:
        """Update index after KV append.

        The reference recomputes only the final incomplete block and new blocks.
        """
        if self._k is None:
            self.build(k)
            return
        if k.shape[0] < self._indexed_tokens:
            raise ValueError("KV cache shrank; rebuild the index")
        if k.shape[1] != self._k.shape[1]:
            raise ValueError("head dimension changed")
        old_complete = self._indexed_tokens // self.cfg.block_size
        start_block = max(0, old_complete - (self._indexed_tokens % self.cfg.block_size != 0))
        start_token = start_block * self.cfg.block_size
        prefix = self.summaries[:start_block]
        suffix = self._pool_all(k[start_token:])
        self._summaries = torch.cat([prefix, suffix], dim=0)
        self._k = k
        self._indexed_tokens = k.shape[0]

    def route(
        self,
        q: torch.Tensor,
        step: int,
        global_positions: Optional[Iterable[int]] = None,
    ) -> Route:
        if self._k is None:
            raise RuntimeError("build or update the index first")
        if q.ndim != 1:
            raise ValueError("q must be [head_dim]")

        if (
            self._cached_route is not None
            and self.cfg.route_refresh > 1
            and step - self._cached_step < self.cfg.route_refresh
        ):
            return Route(
                self._cached_route.token_indices,
                self._cached_route.block_indices,
                reused=True,
            )

        n_tokens = self._k.shape[0]
        local_start = max(0, n_tokens - self.cfg.local_window)
        first_local_block = local_start // self.cfg.block_size

        rq = F.normalize(q.float(), dim=-1)
        scores = self.summaries.float() @ rq
        eligible_count = min(first_local_block, scores.numel())

        if eligible_count:
            budget = min(self.cfg.retrieved_blocks, eligible_count)
            remote_blocks = torch.topk(scores[:eligible_count], k=budget).indices
        else:
            remote_blocks = torch.empty(0, dtype=torch.long, device=self.device)

        parts = [torch.arange(local_start, n_tokens, device=self.device)]
        if self.cfg.global_tokens:
            parts.append(torch.arange(0, min(self.cfg.global_tokens, n_tokens), device=self.device))
        if global_positions is not None:
            gp = torch.as_tensor(list(global_positions), dtype=torch.long, device=self.device)
            parts.append(gp[(gp >= 0) & (gp < n_tokens)])
        if remote_blocks.numel():
            offsets = torch.arange(self.cfg.block_size, device=self.device)
            ids = (remote_blocks[:, None] * self.cfg.block_size + offsets).flatten()
            parts.append(ids[ids < n_tokens])

        token_ids = torch.unique(torch.cat(parts), sorted=True)
        route = Route(token_ids, remote_blocks.sort().values, reused=False)
        self._cached_route = route
        self._cached_step = step
        return route

    def _pool_all(self, k: torch.Tensor) -> torch.Tensor:
        bs = self.cfg.block_size
        n, d = k.shape
        nb = (n + bs - 1) // bs
        padded = nb * bs
        kp = F.pad(k, (0, 0, 0, padded - n)) if padded != n else k
        blocks = kp.view(nb, bs, d)
        lengths = torch.full((nb,), bs, device=k.device, dtype=torch.long)
        lengths[-1] = n - (nb - 1) * bs
        mask = torch.arange(bs, device=k.device)[None, :] < lengths[:, None]
        means = (blocks.float() * mask[..., None]).sum(1) / lengths[:, None]
        return F.normalize(means, dim=-1).to(k.dtype)


def selected_attention(q: torch.Tensor, k: torch.Tensor, v: torch.Tensor, route: Route) -> torch.Tensor:
    """Exact softmax attention over the selected token set."""
    ks = k.index_select(0, route.token_indices)
    vs = v.index_select(0, route.token_indices)
    scores = q @ ks.T / (q.shape[-1] ** 0.5)
    probs = torch.softmax(scores.float(), dim=-1).to(q.dtype)
    return probs @ vs


def dense_attention(q: torch.Tensor, k: torch.Tensor, v: torch.Tensor) -> torch.Tensor:
    scores = q @ k.T / (q.shape[-1] ** 0.5)
    probs = torch.softmax(scores.float(), dim=-1).to(q.dtype)
    return probs @ v
