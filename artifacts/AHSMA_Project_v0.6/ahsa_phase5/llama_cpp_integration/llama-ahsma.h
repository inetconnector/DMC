#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct ggml_context;
struct ggml_tensor;
class llama_kv_cache;
class llama_kv_cache_context;

struct llama_ahsma_params {
    uint32_t block_size       = 64;
    uint32_t local_window     = 2048;
    uint32_t global_tokens    = 64;
    uint32_t retrieved_blocks = 64;
    uint32_t route_refresh    = 8;
    uint32_t route_dim        = 32;
    bool enabled              = false;
};

struct llama_ahsma_route {
    std::vector<uint32_t> block_ids;
    std::vector<uint32_t> token_ids;
    uint64_t generation = 0;
    bool reused = false;
};

// Experimental index owned by the llama context. It never evicts or mutates
// canonical KV data in the initial integration.
class llama_ahsma_index {
public:
    explicit llama_ahsma_index(llama_ahsma_params params);
    ~llama_ahsma_index();

    const llama_ahsma_params & params() const;

    // Called after KV write/append. Reference implementation may read K storage
    // on the host; optimized implementations update summaries on the backend.
    void update_layer(
        const llama_kv_cache & cache,
        int32_t layer,
        uint32_t n_kv);

    // Host-side reference route. q_route is a compact query vector.
    llama_ahsma_route route(
        int32_t layer,
        const float * q_route,
        uint32_t route_dim,
        uint32_t n_kv,
        uint64_t decode_step);

    void clear();

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

// Graph input holding selected logical KV positions. The first integration
// materializes I32 indices; fused backends should later consume block spans.
struct llm_graph_input_ahsma_selection {
    ggml_tensor * token_ids = nullptr; // I32 [n_selected]
    ggml_tensor * block_ids = nullptr; // I32 [n_blocks]
};
