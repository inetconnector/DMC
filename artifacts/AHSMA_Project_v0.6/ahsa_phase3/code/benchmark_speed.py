from __future__ import annotations
import argparse, json, time
import torch
import torch.nn.functional as F
from ahsma_speed import SpeedConfig, PersistentBlockIndex, selected_attention, dense_attention

def sync(device):
    if device.type == "cuda":
        torch.cuda.synchronize(device)

def measure(fn, repeats, device):
    for _ in range(2):
        out = fn()
    sync(device)
    t0 = time.perf_counter()
    for _ in range(repeats):
        out = fn()
    sync(device)
    return (time.perf_counter()-t0)/repeats, out

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--tokens",type=int,default=32768)
    p.add_argument("--head-dim",type=int,default=128)
    p.add_argument("--local",type=int,default=2048)
    p.add_argument("--blocks",type=int,default=64)
    p.add_argument("--block-size",type=int,default=64)
    p.add_argument("--route-refresh",type=int,default=8)
    p.add_argument("--repeats",type=int,default=20)
    p.add_argument("--device",choices=["cpu","cuda"],default="cpu")
    p.add_argument("--dtype",choices=["float32","float16","bfloat16"],default="float32")
    args=p.parse_args()
    if args.device=="cuda" and not torch.cuda.is_available():
        raise SystemExit("CUDA unavailable")
    device=torch.device(args.device)
    dtype=getattr(torch,args.dtype)
    torch.manual_seed(17)
    q=torch.randn(1,args.head_dim,device=device,dtype=dtype)
    k=torch.randn(args.tokens,args.head_dim,device=device,dtype=dtype)
    v=torch.randn_like(k)
    cfg=SpeedConfig(block_size=args.block_size,local_window=args.local,
                    retrieved_blocks=args.blocks,route_refresh=args.route_refresh)
    idx=PersistentBlockIndex(cfg,device)

    build_t,_=measure(lambda: idx.build(k),1,device)
    route_t,route=measure(lambda: idx.route(q[0],step=args.route_refresh*100),args.repeats,device)
    reuse_t,reused=measure(lambda: idx.route(q[0],step=args.route_refresh*100+1),args.repeats,device)
    sparse_t,sparse=measure(lambda: selected_attention(q,k,v,route),args.repeats,device)
    dense_t,dense=measure(lambda: dense_attention(q,k,v),args.repeats,device)

    result={
      "command_parameters":vars(args),
      "device":str(device),
      "index_build_ms":build_t*1000,
      "fresh_route_ms":route_t*1000,
      "reused_route_ms":reuse_t*1000,
      "selected_attention_ms":sparse_t*1000,
      "dense_attention_ms":dense_t*1000,
      "active_tokens":int(route.token_indices.numel()),
      "active_fraction":float(route.token_indices.numel()/args.tokens),
      "attention_only_speedup":float(dense_t/sparse_t),
      "reference_fresh_total_speedup":float(dense_t/(route_t+sparse_t)),
      "reference_reused_total_speedup":float(dense_t/(reuse_t+sparse_t)),
      "output_cosine_random_inputs":float(F.cosine_similarity(dense.float().flatten(),sparse.float().flatten(),dim=0)),
      "warning":"Random-input cosine is a numerical diagnostic, not a model-quality metric. This PyTorch reference gathers K/V and is not a fused production kernel."
    }
    print(json.dumps(result,indent=2))

if __name__=="__main__":
    main()
