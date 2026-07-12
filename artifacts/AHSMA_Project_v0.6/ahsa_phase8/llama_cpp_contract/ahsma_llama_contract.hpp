#pragma once

#include <cstdint>
#include <vector>

#include "../cpu_selected_attention/ahsma_cpu_attention.hpp"
#include "../kv_cache_bridge/ahsma_kv_bridge.hpp"

namespace ahsma {

struct LlamaAhsmaParams {
    std::uint32_t block_size = 64;
    std::uint32_t route_dim = 32;
    std::uint32_t local_window = 2048;
    std::uint32_t global_tokens = 64;
    std::uint32_t retrieved_blocks = 64;
    std::uint32_t route_refresh = 8;
    bool enabled = false;
};

struct LlamaAhsmaSelection {
    std::vector<std::uint32_t> token_ids;
    std::vector<Span> spans;
    bool reused = false;
};

class LlamaAhsmaContract {
public:
    explicit LlamaAhsmaContract(LlamaAhsmaParams params);

    void rebuild(const std::vector<TokenRef> & refs, const std::vector<float> & route_keys);
    void clear();

    LlamaAhsmaSelection select(seq_id_t seq, pos_t current_pos, const std::vector<float> & q, std::uint64_t step);
    std::vector<float> attend_dense(const std::vector<float> & q,
                                    const std::vector<float> & k,
                                    const std::vector<float> & v,
                                    std::size_t head_dim) const;
    std::vector<float> attend_selected(const std::vector<float> & q,
                                       const std::vector<float> & k,
                                       const std::vector<float> & v,
                                       std::size_t head_dim,
                                       const LlamaAhsmaSelection & selection) const;
    std::vector<float> attend_selected_from_spans(const std::vector<float> & q,
                                                  const std::vector<float> & k,
                                                  const std::vector<float> & v,
                                                  std::size_t head_dim,
                                                  const LlamaAhsmaSelection & selection) const;

    const LlamaAhsmaParams & params() const;
    std::size_t block_count() const;

private:
    LlamaAhsmaParams params_;
    KVIndex index_;
};

} // namespace ahsma
