#include "ahsma_llama_gpu_contract.hpp"

#include "../cpu_selected_attention/ahsma_cpu_attention.hpp"

#include <cuda_runtime.h>

#include <cassert>
#include <cstdint>
#include <vector>

using namespace ahsma;

static std::vector<TokenRef> make_refs(std::size_t tokens) {
    std::vector<TokenRef> refs;
    refs.reserve(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        refs.push_back(TokenRef{static_cast<std::uint32_t>(i), static_cast<pos_t>(i), 0});
    }
    return refs;
}

static std::vector<float> make_tensor(std::size_t tokens, std::size_t dim) {
    std::vector<float> out(tokens * dim, 0.0f);
    for (std::size_t t = 0; t < tokens; ++t) {
        for (std::size_t d = 0; d < dim; ++d) {
            out[t * dim + d] = static_cast<float>((t + 2 * d + 5) % 23) * 0.017f;
        }
    }
    return out;
}

int main() {
    int device_count = 0;
    const auto err = cudaGetDeviceCount(&device_count);
    assert(err == cudaSuccess);
    assert(device_count > 0);

    LlamaAhsmaParams params;
    params.block_size = 8;
    params.route_dim = 4;
    params.local_window = 16;
    params.global_tokens = 4;
    params.retrieved_blocks = 2;
    params.route_refresh = 4;
    params.enabled = true;

    LlamaAhsmaGpuContract contract(params);
    const auto refs = make_refs(64);
    const auto route_keys = make_tensor(refs.size(), params.route_dim);
    contract.rebuild(refs, route_keys);

    std::vector<float> q = {1.0f, 0.0f, 0.0f, 0.0f};
    auto selection = contract.select(0, 63, q, 8);
    auto reused = contract.select(0, 63, q, 9);

    assert(!selection.reused);
    assert(reused.reused);
    assert(!selection.token_ids.empty());
    assert(!selection.spans.empty());

    const std::size_t head_dim = 4;
    auto k = make_tensor(64, head_dim);
    auto v = make_tensor(64, head_dim);

    auto dense = contract.attend_dense(q, k, v, head_dim);
    LlamaAhsmaSelection full_selection;
    for (std::uint32_t i = 0; i < 64; ++i) {
        full_selection.token_ids.push_back(i);
    }
    full_selection.spans.push_back(Span{0, 0, 63, 0, 63});

    auto gpu_selected = contract.attend_selected_gpu(q, k, v, head_dim, full_selection);
    auto gpu_spans = contract.attend_selected_from_spans_gpu(q, k, v, head_dim, full_selection);
    auto cpu_selected = contract.attend_selected_cpu(q, k, v, head_dim, full_selection);

    assert(max_abs_diff(dense, gpu_selected) < 1e-5f);
    assert(max_abs_diff(cpu_selected, gpu_selected) < 1e-5f);
    assert(max_abs_diff(gpu_selected, gpu_spans) < 1e-5f);

    return 0;
}
