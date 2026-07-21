#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dmc {

struct DMCConfig {
    std::size_t block_size = 64;
    std::size_t local_window = 2048;
    std::size_t global_tokens = 64;
    std::size_t replay_levels = 4;
};

struct TokenRef {
    std::uint32_t physical_cell = 0;
    std::int64_t logical_pos = 0;
    std::int32_t seq_id = -1;
};

struct Span {
    std::int32_t seq_id = -1;
    std::size_t level = 0;
    std::int64_t first_pos = 0;
    std::int64_t last_pos = 0;
    std::vector<std::uint32_t> token_ids;
};

struct Selection {
    std::vector<std::uint32_t> token_ids;
    std::vector<Span> spans;
};

struct RuntimePlan {
    std::size_t logical_token_count = 0;
    std::vector<std::uint32_t> token_ids;
    std::vector<Span> spans;
    bool compacted = false;
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

inline std::vector<std::uint32_t> expand_spans(const std::vector<Span> & spans) {
    std::vector<std::uint32_t> out;
    std::unordered_set<std::uint32_t> seen;
    for (const auto & span : spans) {
        for (auto id : span.token_ids) {
            if (seen.insert(id).second) {
                out.push_back(id);
            }
        }
    }
    return out;
}

inline std::vector<float> exact_attention(const std::vector<float> & q,
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
    std::vector<float> out(n_queries * head_dim, 0.0f);
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
                out[qi * head_dim + d] += p * vv[d];
            }
        }
    }
    return out;
}

class DMCIndex {
public:
    explicit DMCIndex(DMCConfig cfg) : cfg_(cfg) {
        if (cfg_.block_size == 0) {
            throw std::invalid_argument("block_size must be positive");
        }
    }

    void rebuild(const std::vector<TokenRef> & refs) {
        refs_ = refs;
        std::sort(refs_.begin(), refs_.end(), [](const auto & a, const auto & b) {
            if (a.seq_id != b.seq_id) return a.seq_id < b.seq_id;
            if (a.logical_pos != b.logical_pos) return a.logical_pos < b.logical_pos;
            return a.physical_cell < b.physical_cell;
        });
        refs_by_seq_.clear();
        for (const auto & ref : refs_) {
            refs_by_seq_[ref.seq_id].push_back(ref);
        }
    }

    void clear() {
        refs_.clear();
        refs_by_seq_.clear();
    }

    Selection select(std::int32_t seq_id, std::int64_t current_pos) const {
        const auto it = refs_by_seq_.find(seq_id);
        if (it == refs_by_seq_.end() || it->second.empty()) {
            throw std::runtime_error("sequence is empty");
        }
        const auto & seq_refs = it->second;
        const auto current_it = std::upper_bound(
            seq_refs.begin(),
            seq_refs.end(),
            current_pos,
            [](std::int64_t value, const TokenRef & ref) {
                return value < ref.logical_pos;
            });
        std::vector<TokenRef> causal_refs(seq_refs.begin(), current_it);
        if (causal_refs.empty()) {
            throw std::runtime_error("sequence is empty");
        }

        std::vector<Span> spans;
        const std::int64_t local_start = current_pos >= static_cast<std::int64_t>(cfg_.local_window)
            ? current_pos - static_cast<std::int64_t>(cfg_.local_window) + 1
            : 0;

        if (cfg_.local_window > 0) {
            spans.push_back(build_span(seq_id, causal_refs, local_start, current_pos, 0));
        }
        if (cfg_.global_tokens > 0) {
            const std::int64_t global_end = std::min<std::int64_t>(
                static_cast<std::int64_t>(cfg_.global_tokens) - 1, current_pos);
            if (global_end >= 0) {
                spans.push_back(build_span(seq_id, causal_refs, 0, global_end, 0));
            }
        }

        const std::size_t replay_stride = std::max(cfg_.local_window, cfg_.block_size);
        for (std::size_t level = 0; level < cfg_.replay_levels; ++level) {
            if (level >= std::numeric_limits<std::size_t>::digits ||
                replay_stride > (std::numeric_limits<std::size_t>::max() >> level) ||
                cfg_.block_size > (std::numeric_limits<std::size_t>::max() >> level)) {
                break;
            }
            const std::size_t distance = replay_stride << level;
            if (distance > static_cast<std::size_t>(current_pos)) {
                break;
            }
            const std::int64_t end = current_pos - static_cast<std::int64_t>(distance);
            const std::size_t span_size = cfg_.block_size << level;
            const std::int64_t start = std::max<std::int64_t>(0, end - static_cast<std::int64_t>(span_size) + 1);
            spans.push_back(build_span(seq_id, causal_refs, start, end, level + 1));
        }

        Selection out;
        out.spans = spans;
        std::unordered_set<std::uint32_t> seen;
        for (const auto & span : spans) {
            for (auto id : span.token_ids) {
                if (seen.insert(id).second) {
                    out.token_ids.push_back(id);
                }
            }
        }
        return out;
    }

private:
    DMCConfig cfg_;
    std::vector<TokenRef> refs_;
    std::unordered_map<std::int32_t, std::vector<TokenRef>> refs_by_seq_;

    static Span build_span(std::int32_t seq_id,
                           const std::vector<TokenRef> & refs,
                           std::int64_t start_pos,
                           std::int64_t end_pos,
                           std::size_t level) {
        if (start_pos > end_pos) {
            throw std::invalid_argument("invalid span bounds");
        }
        const auto start_it = std::lower_bound(
            refs.begin(),
            refs.end(),
            start_pos,
            [](const TokenRef & ref, std::int64_t value) {
                return ref.logical_pos < value;
            });
        const auto end_it = std::upper_bound(
            refs.begin(),
            refs.end(),
            end_pos,
            [](std::int64_t value, const TokenRef & ref) {
                return value < ref.logical_pos;
            });
        Span span;
        span.seq_id = seq_id;
        span.level = level;
        span.first_pos = start_pos;
        span.last_pos = end_pos;
        for (auto it = start_it; it != end_it; ++it) {
            span.token_ids.push_back(it->physical_cell);
        }
        if (span.token_ids.empty()) {
            throw std::runtime_error("selected span is empty");
        }
        return span;
    }
};

// Builds the deterministic token plan consumed by an inference runtime. Short
// histories remain dense; histories above the physical budget use the same
// multiresolution selector as the reference attention path. Returned token IDs
// are sorted chronologically because a decoder must rehydrate KV state in
// causal order even though attention itself is insensitive to selection order.
inline RuntimePlan plan_runtime_context(
        std::size_t token_count,
        std::size_t physical_token_budget,
        DMCConfig cfg = DMCConfig{}) {
    if (physical_token_budget == 0) {
        throw std::invalid_argument("physical_token_budget must be positive");
    }
    if (token_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::overflow_error("DMC runtime history exceeds 32-bit token addressing");
    }

    RuntimePlan plan;
    plan.logical_token_count = token_count;
    if (token_count == 0) {
        return plan;
    }

    if (token_count <= physical_token_budget) {
        plan.token_ids.reserve(token_count);
        for (std::size_t i = 0; i < token_count; ++i) {
            plan.token_ids.push_back(static_cast<std::uint32_t>(i));
        }
        return plan;
    }

    std::vector<TokenRef> refs;
    refs.reserve(token_count);
    for (std::size_t i = 0; i < token_count; ++i) {
        refs.push_back(TokenRef{
            static_cast<std::uint32_t>(i),
            static_cast<std::int64_t>(i),
            0,
        });
    }

    DMCIndex index(cfg);
    index.rebuild(refs);
    auto selection = index.select(0, static_cast<std::int64_t>(token_count - 1));
    plan.spans = selection.spans;
    plan.token_ids = std::move(selection.token_ids);
    std::sort(plan.token_ids.begin(), plan.token_ids.end());
    plan.token_ids.erase(
        std::unique(plan.token_ids.begin(), plan.token_ids.end()),
        plan.token_ids.end());

    // A constrained physical runtime still gives priority to the global prefix
    // and recent local window. Remaining replay tokens fill the free budget in
    // deterministic chronological order.
    if (plan.token_ids.size() > physical_token_budget) {
        std::vector<std::uint32_t> bounded;
        bounded.reserve(physical_token_budget);
        std::unordered_set<std::uint32_t> seen;

        const std::size_t global_keep = std::min({
            cfg.global_tokens,
            token_count,
            physical_token_budget / 4,
        });
        for (std::size_t i = 0; i < global_keep; ++i) {
            const auto id = static_cast<std::uint32_t>(i);
            if (seen.insert(id).second) {
                bounded.push_back(id);
            }
        }

        const std::size_t local_capacity = physical_token_budget - bounded.size();
        const std::size_t local_keep = std::min({
            cfg.local_window,
            token_count,
            local_capacity,
        });
        const std::size_t local_begin = token_count - local_keep;
        for (std::size_t i = local_begin; i < token_count; ++i) {
            const auto id = static_cast<std::uint32_t>(i);
            if (seen.insert(id).second) {
                bounded.push_back(id);
            }
        }

        for (const auto id : plan.token_ids) {
            if (bounded.size() == physical_token_budget) {
                break;
            }
            if (seen.insert(id).second) {
                bounded.push_back(id);
            }
        }

        std::sort(bounded.begin(), bounded.end());
        plan.token_ids = std::move(bounded);
    }

    if (plan.token_ids.empty() || plan.token_ids.back() != token_count - 1) {
        throw std::runtime_error("DMC runtime plan does not contain the newest token");
    }
    plan.compacted = true;
    return plan;
}

} // namespace dmc
