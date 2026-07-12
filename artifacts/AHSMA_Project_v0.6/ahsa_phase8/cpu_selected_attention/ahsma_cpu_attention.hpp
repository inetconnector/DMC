#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ahsma {

struct CellSpan {
    std::uint32_t first_cell = 0;
    std::uint32_t last_cell = 0;
};

struct AttentionResult {
    std::vector<float> output;
    std::vector<std::uint32_t> token_ids;
};

inline void validate_qkv(const std::vector<float> & q,
                         const std::vector<float> & k,
                         const std::vector<float> & v,
                         std::size_t head_dim) {
    if (head_dim == 0) {
        throw std::invalid_argument("head_dim must be positive");
    }
    if (q.size() % head_dim != 0) {
        throw std::invalid_argument("q shape mismatch");
    }
    if (k.size() != v.size()) {
        throw std::invalid_argument("k and v must have identical storage");
    }
    if (k.size() % head_dim != 0) {
        throw std::invalid_argument("k shape mismatch");
    }
}

inline std::vector<std::uint32_t> expand_spans(const std::vector<CellSpan> & spans) {
    std::vector<std::uint32_t> out;
    for (const auto & span : spans) {
        if (span.last_cell < span.first_cell) {
            throw std::invalid_argument("invalid span");
        }
        for (std::uint32_t i = span.first_cell; i <= span.last_cell; ++i) {
            out.push_back(i);
        }
    }
    return out;
}

inline std::vector<float> dense_attention(const std::vector<float> & q,
                                          const std::vector<float> & k,
                                          const std::vector<float> & v,
                                          std::size_t head_dim) {
    validate_qkv(q, k, v, head_dim);
    const std::size_t n_queries = q.size() / head_dim;
    const std::size_t n_tokens = k.size() / head_dim;
    std::vector<float> out(n_queries * head_dim, 0.0f);
    for (std::size_t qi = 0; qi < n_queries; ++qi) {
        const float * qv = &q[qi * head_dim];
        std::vector<float> scores(n_tokens, 0.0f);
        float max_score = -std::numeric_limits<float>::infinity();
        for (std::size_t ti = 0; ti < n_tokens; ++ti) {
            const float * kv = &k[ti * head_dim];
            float s = 0.0f;
            for (std::size_t d = 0; d < head_dim; ++d) {
                s += qv[d] * kv[d];
            }
            s /= std::sqrt(static_cast<float>(head_dim));
            scores[ti] = s;
            max_score = std::max(max_score, s);
        }
        float denom = 0.0f;
        for (float & s : scores) {
            s = std::exp(s - max_score);
            denom += s;
        }
        for (std::size_t ti = 0; ti < n_tokens; ++ti) {
            const float p = scores[ti] / denom;
            const float * vv = &v[ti * head_dim];
            for (std::size_t d = 0; d < head_dim; ++d) {
                out[qi * head_dim + d] += p * vv[d];
            }
        }
    }
    return out;
}

inline AttentionResult exact_selected_attention(const std::vector<float> & q,
                                                const std::vector<float> & k,
                                                const std::vector<float> & v,
                                                const std::vector<std::uint32_t> & token_ids,
                                                std::size_t head_dim) {
    validate_qkv(q, k, v, head_dim);
    if (token_ids.empty()) {
        throw std::invalid_argument("token_ids must not be empty");
    }

    const std::size_t n_queries = q.size() / head_dim;
    const std::size_t n_tokens = k.size() / head_dim;
    AttentionResult result;
    result.token_ids = token_ids;
    result.output.assign(n_queries * head_dim, 0.0f);

    for (std::size_t qi = 0; qi < n_queries; ++qi) {
        const float * qv = &q[qi * head_dim];
        std::vector<float> scores(token_ids.size(), 0.0f);
        float max_score = -std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < token_ids.size(); ++i) {
            const std::uint32_t tid = token_ids[i];
            if (tid >= n_tokens) {
                throw std::out_of_range("token id out of range");
            }
            const float * kv = &k[static_cast<std::size_t>(tid) * head_dim];
            float s = 0.0f;
            for (std::size_t d = 0; d < head_dim; ++d) {
                s += qv[d] * kv[d];
            }
            s /= std::sqrt(static_cast<float>(head_dim));
            scores[i] = s;
            max_score = std::max(max_score, s);
        }
        float denom = 0.0f;
        for (float & s : scores) {
            s = std::exp(s - max_score);
            denom += s;
        }
        for (std::size_t i = 0; i < token_ids.size(); ++i) {
            const float p = scores[i] / denom;
            const float * vv = &v[static_cast<std::size_t>(token_ids[i]) * head_dim];
            for (std::size_t d = 0; d < head_dim; ++d) {
                result.output[qi * head_dim + d] += p * vv[d];
            }
        }
    }

    return result;
}

inline AttentionResult exact_selected_attention_from_spans(const std::vector<float> & q,
                                                           const std::vector<float> & k,
                                                           const std::vector<float> & v,
                                                           const std::vector<CellSpan> & spans,
                                                           std::size_t head_dim) {
    auto token_ids = expand_spans(spans);
    return exact_selected_attention(q, k, v, token_ids, head_dim);
}

inline float max_abs_diff(const std::vector<float> & a, const std::vector<float> & b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("vector size mismatch");
    }
    float out = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        out = std::max(out, std::abs(a[i] - b[i]));
    }
    return out;
}

} // namespace ahsma
