#include "ahsma_cpu_attention.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace ahsma;

static std::vector<float> make_tensor(std::size_t tokens, std::size_t dim) {
    std::vector<float> out(tokens * dim, 0.0f);
    for (std::size_t t = 0; t < tokens; ++t) {
        for (std::size_t d = 0; d < dim; ++d) {
            out[t * dim + d] = static_cast<float>((t + 1) * (d + 1)) * 0.01f;
        }
    }
    return out;
}

int main() {
    const std::size_t head_dim = 8;
    const std::size_t tokens = 32;
    std::vector<float> q = {
        0.2f, 0.1f, 0.0f, 0.4f, -0.2f, 0.3f, 0.5f, -0.1f,
    };
    auto k = make_tensor(tokens, head_dim);
    auto v = make_tensor(tokens, head_dim);

    std::vector<std::uint32_t> all_ids(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        all_ids[i] = static_cast<std::uint32_t>(i);
    }

    auto dense = dense_attention(q, k, v, head_dim);
    auto selected = exact_selected_attention(q, k, v, all_ids, head_dim);
    assert(max_abs_diff(dense, selected.output) < 1e-5f);

    std::vector<CellSpan> spans = {{0, 7}, {8, 15}, {16, 31}};
    auto from_spans = exact_selected_attention_from_spans(q, k, v, spans, head_dim);
    assert(max_abs_diff(selected.output, from_spans.output) < 1e-5f);
    assert(from_spans.token_ids.size() == tokens);

    return 0;
}
