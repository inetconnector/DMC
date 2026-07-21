import torch

from dmc import DMCConfig, DMCIndex, TokenRef, dense_attention, exact_attention
from dmc.core import exact_attention_from_selection


def _make_refs(n_tokens: int, seq_id: int = 0) -> list[TokenRef]:
    return [
        TokenRef(physical_cell=i, logical_pos=i, seq_id=seq_id)
        for i in range(n_tokens)
    ]


def test_full_selection_matches_dense():
    torch.manual_seed(1)
    q = torch.randn(2, 16)
    k = torch.randn(64, 16)
    v = torch.randn(64, 16)
    ids = torch.arange(64)
    torch.testing.assert_close(exact_attention(q, k, v, ids), dense_attention(q, k, v))


def test_dmc_selection_is_causal_and_multiresolution():
    index = DMCIndex(DMCConfig(block_size=8, local_window=16, global_tokens=4, replay_levels=3))
    index.rebuild(_make_refs(64))

    selection = index.select(seq_id=0, current_pos=47)
    chosen = selection.token_ids.tolist()

    assert max(chosen) < 48
    assert set(range(4)).issubset(set(chosen))
    assert set(range(32, 48)).issubset(set(chosen))
    assert any(span.last_pos < 32 for span in selection.spans[2:])


def test_span_lengths_follow_the_hierarchy():
    index = DMCIndex(DMCConfig(block_size=8, local_window=8, global_tokens=0, replay_levels=3))
    index.rebuild(_make_refs(128))

    selection = index.select(seq_id=0, current_pos=95)
    replay_spans = selection.spans[1:]

    assert replay_spans[0].last_pos - replay_spans[0].first_pos + 1 == 8
    assert replay_spans[1].last_pos - replay_spans[1].first_pos + 1 == 16
    assert replay_spans[2].last_pos - replay_spans[2].first_pos + 1 == 32
    assert [(span.first_pos, span.last_pos) for span in replay_spans] == [
        (80, 87),
        (64, 79),
        (32, 63),
    ]


def test_selection_stays_isolated_per_sequence():
    index = DMCIndex(DMCConfig(block_size=4, local_window=8, global_tokens=0, replay_levels=0))
    refs = [
        TokenRef(physical_cell=0, logical_pos=0, seq_id=0),
        TokenRef(physical_cell=1, logical_pos=0, seq_id=1),
        TokenRef(physical_cell=2, logical_pos=1, seq_id=0),
        TokenRef(physical_cell=3, logical_pos=1, seq_id=1),
    ]
    index.rebuild(refs)

    sel0 = index.select(seq_id=0, current_pos=1)
    sel1 = index.select(seq_id=1, current_pos=1)

    assert set(sel0.token_ids.tolist()) == {0, 2}
    assert set(sel1.token_ids.tolist()) == {1, 3}


def test_selection_attention_matches_dense_when_everything_is_selected():
    torch.manual_seed(2)
    q = torch.randn(2, 32)
    k = torch.randn(128, 32)
    v = torch.randn(128, 32)

    index = DMCIndex(DMCConfig(block_size=8, local_window=128, global_tokens=0, replay_levels=0))
    index.rebuild(_make_refs(128))
    selection = index.select(seq_id=0, current_pos=127)

    sparse = exact_attention_from_selection(q, k, v, selection)
    dense = dense_attention(q, k, v)
    torch.testing.assert_close(sparse, dense)
