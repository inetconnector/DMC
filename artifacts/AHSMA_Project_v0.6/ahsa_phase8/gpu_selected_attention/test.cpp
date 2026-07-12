#include "ahsma_gpu_attention.cuh"

#include "../cpu_selected_attention/ahsma_cpu_attention.hpp"
#include "../kv_cache_bridge/ahsma_kv_bridge.hpp"

#include <cuda_runtime.h>

#include <cassert>
#include <cstdint>
#include <vector>

using namespace ahsma;

static std::vector<float> make_tensor(std::size_t tokens, std::size_t dim) {
    std::vector<float> out(tokens * dim, 0.0f);
    for (std::size_t t = 0; t < tokens; ++t) {
        for (std::size_t d = 0; d < dim; ++d) {
            out[t * dim + d] = static_cast<float>((t + 3 * d + 1) % 19) * 0.021f;
        }
    }
    return out;
}

int main() {
    int device_count = 0;
    const auto err = cudaGetDeviceCount(&device_count);
    assert(err == cudaSuccess);
    assert(device_count > 0);

    const std::size_t tokens = 128;
    const std::size_t head_dim = 32;
    const std::size_t route_dim = 16;

    Config cfg;
    cfg.block_size = 16;
    cfg.route_dim = static_cast<std::uint32_t>(route_dim);
    cfg.local_window = 32;
    cfg.global_tokens = 8;
    cfg.retrieved_blocks = 2;
    cfg.route_refresh = 4;

    std::vector<TokenRef> refs;
    refs.reserve(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        refs.push_back(TokenRef{static_cast<std::uint32_t>(i), static_cast<pos_t>(i), 0});
    }

    auto route_keys = make_tensor(tokens, route_dim);
    KVIndex idx(cfg);
    idx.rebuild(refs, route_keys);

    std::vector<float> route_query(route_dim, 0.0f);
    route_query[0] = 1.0f;
    auto route = idx.route(0, static_cast<pos_t>(tokens - 1), route_query, 8);
    assert(!route.cells.empty());

    auto q = make_tensor(1, head_dim);
    auto k = make_tensor(tokens, head_dim);
    auto v = make_tensor(tokens, head_dim);

    std::vector<std::uint32_t> all_ids(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        all_ids[i] = static_cast<std::uint32_t>(i);
    }

    auto cpu_dense = dense_attention(q, k, v, head_dim);
    auto gpu_dense = run_selected_attention_cuda(q, k, v, all_ids, head_dim);
    assert(max_abs_diff(cpu_dense, gpu_dense.output) < 1e-4f);

    auto cpu_sparse = exact_selected_attention(q, k, v, route.cells, head_dim);
    auto gpu_sparse = run_selected_attention_cuda(q, k, v, route.cells, head_dim);
    assert(max_abs_diff(cpu_sparse.output, gpu_sparse.output) < 1e-4f);

    return 0;
}
