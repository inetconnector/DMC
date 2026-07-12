import torch

from ahsma_reference import AHSMAConfig, dense_attention, exact_sparse_attention, make_block_summaries, select_tokens


def test_block_summaries_partial_block():
    k = torch.arange(30, dtype=torch.float32).reshape(10, 3)
    s, lengths = make_block_summaries(k, 4)
    assert s.shape == (3, 3)
    assert lengths.tolist() == [4, 4, 2]


def test_selection_is_causal_and_contains_local_and_global():
    torch.manual_seed(1)
    k = torch.randn(100, 16)
    q = torch.randn(16)
    cfg = AHSMAConfig(block_size=8, local_window=16, global_tokens=4, retrieved_blocks=2,
                      min_retrieved_blocks=2, max_retrieved_blocks=2, adaptive_budget=False)
    result = select_tokens(q, k, cfg)
    chosen = set(result.token_indices.tolist())
    assert set(range(84, 100)).issubset(chosen)
    assert set(range(4)).issubset(chosen)
    assert max(chosen) < 100


def test_full_selection_matches_dense():
    torch.manual_seed(2)
    q = torch.randn(2, 32)
    k = torch.randn(128, 32)
    v = torch.randn(128, 32)
    ids = torch.arange(128)
    dense = dense_attention(q, k, v)
    sparse = exact_sparse_attention(q, k, v, ids)
    torch.testing.assert_close(dense, sparse)
