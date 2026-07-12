#include "ahsma_kv_index.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

using namespace ahsma;

static std::vector<float> keys(size_t n, size_t d) {
    std::vector<float> out(n*d, 0.0f);
    for (size_t i=0;i<n;++i) out[i*d+(i%d)] = 1.0f;
    return out;
}

int main() {
    Config c;
    c.block_size=8; c.route_dim=4; c.local_window=16;
    c.global_tokens=4; c.retrieved_blocks=2; c.route_refresh=4;
    KVIndex idx(c);

    std::vector<TokenRef> refs;
    for (uint32_t i=0;i<64;++i) refs.push_back({i,(pos_t)i,0});
    for (uint32_t i=64;i<96;++i) refs.push_back({i,(pos_t)(i-64),1});
    idx.rebuild(refs, keys(refs.size(), c.route_dim));
    assert(idx.block_count()==12);

    std::vector<float> q={1,0,0,0};
    auto a=idx.route(0,63,q,8);
    auto b=idx.route(0,63,q,9);
    assert(!a.reused && b.reused);
    assert(std::find(a.cells.begin(),a.cells.end(),63)!=a.cells.end());

    auto g0=idx.generation();
    idx.seq_rm(0,8,16);
    assert(idx.generation()>g0);
    auto c1=idx.route(0,63,q,10);
    assert(!c1.reused);

    idx.seq_cp(1,2,0,-1);
    auto r2=idx.route(2,31,q,20);
    assert(!r2.cells.empty());

    idx.seq_add(2,0,-1,100);
    auto r3=idx.route(2,131,q,24);
    assert(!r3.cells.empty());

    idx.seq_keep(2);
    auto r4=idx.route(2,131,q,28);
    assert(!r4.cells.empty());

    idx.clear();
    assert(idx.block_count()==0);

    std::cout << "all lifecycle tests passed\n";
    return 0;
}
