#include "ahsma_index.hpp"
#include <cassert>
#include <vector>
int main() {
    ahsma::Config c; c.block_size=8; c.local_window=16; c.global_tokens=4;
    c.retrieved_blocks=2; c.route_refresh=4;
    const std::size_t n=100,d=4;
    std::vector<float> k(n*d,0.0f),q(d,1.0f);
    for (std::size_t t=0;t<n;++t) k[t*d+(t%d)] = 1.0f;
    ahsma::BlockIndex idx(c,d);
    idx.build(k,n);
    assert(idx.block_count()==13);
    auto a=idx.route(q,8);
    auto b=idx.route(q,9);
    assert(!a.reused && b.reused);
    for (std::uint32_t i=84;i<100;++i) {
        assert(std::find(a.token_ids.begin(),a.token_ids.end(),i)!=a.token_ids.end());
    }
    return 0;
}
