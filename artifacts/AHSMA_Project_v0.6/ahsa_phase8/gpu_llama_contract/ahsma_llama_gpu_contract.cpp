#include "ahsma_llama_gpu_contract.hpp"

#include <stdexcept>

namespace ahsma {

LlamaAhsmaGpuContract::LlamaAhsmaGpuContract(LlamaAhsmaParams params)
    : params_(params)
    , index_(Config{
          params.block_size,
          params.route_dim,
          params.local_window,
          params.global_tokens,
          params.retrieved_blocks,
          params.route_refresh})
{
    if (params_.block_size == 0 || params_.route_dim == 0) {
        throw std::invalid_argument("invalid llama AHSMA params");
    }
}

void LlamaAhsmaGpuContract::rebuild(const std::vector<TokenRef> & refs, const std::vector<float> & route_keys) {
    index_.rebuild(refs, route_keys);
}

void LlamaAhsmaGpuContract::clear() {
    index_.clear();
}

LlamaAhsmaSelection LlamaAhsmaGpuContract::select(seq_id_t seq, pos_t current_pos, const std::vector<float> & q, std::uint64_t step) {
    auto route = index_.route(seq, current_pos, q, step);
    return LlamaAhsmaSelection{route.cells, route.spans, route.reused};
}

std::vector<float> LlamaAhsmaGpuContract::attend_dense(const std::vector<float> & q,
                                                       const std::vector<float> & k,
                                                       const std::vector<float> & v,
                                                       std::size_t head_dim) const {
    return dense_attention(q, k, v, head_dim);
}

std::vector<float> LlamaAhsmaGpuContract::attend_selected_gpu(const std::vector<float> & q,
                                                              const std::vector<float> & k,
                                                              const std::vector<float> & v,
                                                              std::size_t head_dim,
                                                              const LlamaAhsmaSelection & selection) const {
    return run_selected_attention_cuda(q, k, v, selection.token_ids, head_dim).output;
}

std::vector<float> LlamaAhsmaGpuContract::attend_selected_from_spans_gpu(const std::vector<float> & q,
                                                                         const std::vector<float> & k,
                                                                         const std::vector<float> & v,
                                                                         std::size_t head_dim,
                                                                         const LlamaAhsmaSelection & selection) const {
    std::vector<CellSpan> spans;
    spans.reserve(selection.spans.size());
    for (const auto & span : selection.spans) {
        spans.push_back(CellSpan{span.first_cell, span.last_cell});
    }
    return run_selected_attention_cuda(q, k, v, expand_spans(spans), head_dim).output;
}

std::vector<float> LlamaAhsmaGpuContract::attend_selected_cpu(const std::vector<float> & q,
                                                              const std::vector<float> & k,
                                                              const std::vector<float> & v,
                                                              std::size_t head_dim,
                                                              const LlamaAhsmaSelection & selection) const {
    return exact_selected_attention(q, k, v, selection.token_ids, head_dim).output;
}

const LlamaAhsmaParams & LlamaAhsmaGpuContract::params() const {
    return params_;
}

std::size_t LlamaAhsmaGpuContract::block_count() const {
    return index_.block_count();
}

} // namespace ahsma
