#pragma once

#include "../llama_cpp_contract/ahsma_llama_contract.hpp"
#include "../gpu_selected_attention/ahsma_gpu_attention.cuh"

namespace ahsma {

class LlamaAhsmaGpuContract {
public:
    explicit LlamaAhsmaGpuContract(LlamaAhsmaParams params);

    void rebuild(const std::vector<TokenRef> & refs, const std::vector<float> & route_keys);
    void clear();

    LlamaAhsmaSelection select(seq_id_t seq, pos_t current_pos, const std::vector<float> & q, std::uint64_t step);
    std::vector<float> attend_dense(const std::vector<float> & q,
                                    const std::vector<float> & k,
                                    const std::vector<float> & v,
                                    std::size_t head_dim) const;
    std::vector<float> attend_selected_gpu(const std::vector<float> & q,
                                           const std::vector<float> & k,
                                           const std::vector<float> & v,
                                           std::size_t head_dim,
                                           const LlamaAhsmaSelection & selection) const;
    std::vector<float> attend_selected_from_spans_gpu(const std::vector<float> & q,
                                                      const std::vector<float> & k,
                                                      const std::vector<float> & v,
                                                      std::size_t head_dim,
                                                      const LlamaAhsmaSelection & selection) const;
    std::vector<float> attend_selected_cpu(const std::vector<float> & q,
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
