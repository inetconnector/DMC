import torch
from ahsma_speed import SpeedConfig, PersistentBlockIndex, selected_attention, dense_attention

def test_full_selection_equals_dense():
    torch.manual_seed(1)
    q=torch.randn(2,16); k=torch.randn(64,16); v=torch.randn(64,16)
    cfg=SpeedConfig(block_size=8,local_window=64,global_tokens=0,retrieved_blocks=0)
    idx=PersistentBlockIndex(cfg,"cpu"); idx.build(k)
    route=idx.route(q[-1],step=0)
    torch.testing.assert_close(selected_attention(q,k,v,route),dense_attention(q,k,v))

def test_route_reuse():
    k=torch.randn(128,16); q=torch.randn(16)
    idx=PersistentBlockIndex(SpeedConfig(block_size=8,local_window=16,retrieved_blocks=2,route_refresh=4),"cpu")
    idx.build(k)
    a=idx.route(q,step=8); b=idx.route(q,step=9)
    assert not a.reused and b.reused
    assert torch.equal(a.token_indices,b.token_indices)

def test_incremental_update_changes_block_count():
    k=torch.randn(65,16)
    idx=PersistentBlockIndex(SpeedConfig(block_size=16),"cpu")
    idx.build(k[:31])
    assert idx.summaries.shape[0]==2
    idx.update(k)
    assert idx.summaries.shape[0]==5
