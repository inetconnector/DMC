#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ahsma {

struct CudaAttentionResult {
    std::vector<float> output;
    float kernel_ms = 0.0f;
};

CudaAttentionResult run_selected_attention_cuda(
    const std::vector<float> & q,
    const std::vector<float> & k,
    const std::vector<float> & v,
    const std::vector<std::uint32_t> & token_ids,
    std::size_t head_dim);

} // namespace ahsma
