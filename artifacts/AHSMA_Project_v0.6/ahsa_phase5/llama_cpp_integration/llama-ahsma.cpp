#include "llama-ahsma.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

struct llama_ahsma_index::impl {
    explicit impl(llama_ahsma_params p) : cfg(p) {}

    struct layer_state {
        std::vector<float> summaries; // [n_blocks, route_dim]
        uint32_t n_kv = 0;
        llama_ahsma_route cached;
        uint64_t cached_step = 0;
        bool cache_valid = false;
    };

    llama_ahsma_params cfg;
    std::unordered_map<int32_t, layer_state> layers;
};

llama_ahsma_index::llama_ahsma_index(llama_ahsma_params params)
    : pimpl(new impl(params)) {
    if (params.block_size == 0 || params.route_dim == 0) {
        throw std::invalid_argument("invalid AHSMA parameters");
    }
}

llama_ahsma_index::~llama_ahsma_index() = default;

const llama_ahsma_params & llama_ahsma_index::params() const {
    return pimpl->cfg;
}

void llama_ahsma_index::update_layer(
        const llama_kv_cache &,
        int32_t layer,
        uint32_t n_kv) {
    // Integration checkpoint:
    // 1. obtain layer K storage via llama_kv_cache::get_k_storage(layer)
    // 2. update only the final incomplete and newly completed block summaries
    // 3. keep summaries on the same backend as the routing operation
    //
    // This source intentionally does not read backend tensor storage directly;
    // that must use llama.cpp backend APIs and layer/head layout metadata.
    auto & state = pimpl->layers[layer];
    state.n_kv = n_kv;
    state.cache_valid = false;
}

llama_ahsma_route llama_ahsma_index::route(
        int32_t layer,
        const float *,
        uint32_t route_dim,
        uint32_t n_kv,
        uint64_t decode_step) {
    if (route_dim != pimpl->cfg.route_dim) {
        throw std::invalid_argument("AHSMA routing dimension mismatch");
    }

    auto & state = pimpl->layers[layer];
    if (state.cache_valid &&
        decode_step >= state.cached_step &&
        decode_step - state.cached_step < pimpl->cfg.route_refresh) {
        auto out = state.cached;
        out.reused = true;
        return out;
    }

    // Safe reference selection until backend summaries are wired:
    // preserve local and global paths. Retrieved blocks are deliberately not
    // fabricated, because doing so would create an unvalidated quality path.
    llama_ahsma_route out;
    const uint32_t local_begin =
        n_kv > pimpl->cfg.local_window ? n_kv - pimpl->cfg.local_window : 0;

    std::vector<uint8_t> selected(n_kv, 0);
    for (uint32_t i = local_begin; i < n_kv; ++i) selected[i] = 1;
    for (uint32_t i = 0; i < std::min(n_kv, pimpl->cfg.global_tokens); ++i) selected[i] = 1;
    for (uint32_t i = 0; i < n_kv; ++i) {
        if (selected[i]) out.token_ids.push_back(i);
    }

    out.generation = decode_step;
    state.cached = out;
    state.cached_step = decode_step;
    state.cache_valid = true;
    return out;
}

void llama_ahsma_index::clear() {
    pimpl->layers.clear();
}
