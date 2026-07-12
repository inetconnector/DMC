#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ahsma {

struct Config {
    std::size_t block_size       = 64;
    std::size_t local_window     = 2048;
    std::size_t global_tokens    = 64;
    std::size_t retrieved_blocks = 64;
    std::size_t route_refresh    = 8;
};

struct Route {
    std::vector<std::uint32_t> token_ids;
    std::vector<std::uint32_t> block_ids;
    bool reused = false;
};

class BlockIndex {
public:
    BlockIndex(Config cfg, std::size_t head_dim)
        : cfg_(cfg), head_dim_(head_dim) {
        if (cfg_.block_size == 0 || head_dim_ == 0) {
            throw std::invalid_argument("block_size and head_dim must be positive");
        }
    }

    void build(const std::vector<float> & keys, std::size_t n_tokens) {
        validate_keys(keys, n_tokens);
        keys_ = &keys;
        n_tokens_ = n_tokens;
        summaries_ = pool_range(keys, 0, n_tokens);
        cached_valid_ = false;
    }

    void update(const std::vector<float> & keys, std::size_t n_tokens) {
        validate_keys(keys, n_tokens);
        if (n_tokens < n_tokens_) {
            throw std::invalid_argument("KV cache shrank; rebuild required");
        }
        const std::size_t old_complete = n_tokens_ / cfg_.block_size;
        const bool had_partial = (n_tokens_ % cfg_.block_size) != 0;
        const std::size_t start_block = had_partial && old_complete > 0 ? old_complete : old_complete;
        const std::size_t start_token = start_block * cfg_.block_size;

        std::vector<float> suffix = pool_range(keys, start_token, n_tokens);
        summaries_.resize(start_block * head_dim_);
        summaries_.insert(summaries_.end(), suffix.begin(), suffix.end());
        keys_ = &keys;
        n_tokens_ = n_tokens;
        cached_valid_ = false;
    }

    Route route(const std::vector<float> & query, std::size_t step) {
        if (query.size() != head_dim_) {
            throw std::invalid_argument("query dimension mismatch");
        }
        if (keys_ == nullptr) {
            throw std::runtime_error("index is empty");
        }
        if (cached_valid_ && cfg_.route_refresh > 1 &&
            step >= cached_step_ && step - cached_step_ < cfg_.route_refresh) {
            Route r = cached_route_;
            r.reused = true;
            return r;
        }

        const std::size_t local_start =
            n_tokens_ > cfg_.local_window ? n_tokens_ - cfg_.local_window : 0;
        const std::size_t eligible_blocks = std::min(
            local_start / cfg_.block_size, block_count());

        std::vector<std::pair<float, std::uint32_t>> scored;
        scored.reserve(eligible_blocks);
        const float qnorm = norm(query.data());
        for (std::size_t b = 0; b < eligible_blocks; ++b) {
            const float * s = &summaries_[b * head_dim_];
            float dot = 0.0f;
            for (std::size_t d = 0; d < head_dim_; ++d) dot += query[d] * s[d];
            scored.emplace_back(dot / std::max(qnorm, 1e-12f), static_cast<std::uint32_t>(b));
        }

        const std::size_t keep = std::min(cfg_.retrieved_blocks, scored.size());
        if (keep < scored.size()) {
            std::nth_element(scored.begin(), scored.begin()+keep, scored.end(),
                [](const auto & a, const auto & b){ return a.first > b.first; });
            scored.resize(keep);
        }
        std::sort(scored.begin(), scored.end(),
            [](const auto & a, const auto & b){ return a.second < b.second; });

        Route out;
        out.block_ids.reserve(scored.size());
        std::vector<std::uint8_t> selected(n_tokens_, 0);

        auto mark = [&](std::size_t begin, std::size_t end) {
            end = std::min(end, n_tokens_);
            for (std::size_t i = begin; i < end; ++i) selected[i] = 1;
        };
        mark(local_start, n_tokens_);
        mark(0, cfg_.global_tokens);
        for (const auto & item : scored) {
            out.block_ids.push_back(item.second);
            const std::size_t begin = static_cast<std::size_t>(item.second) * cfg_.block_size;
            mark(begin, begin + cfg_.block_size);
        }
        for (std::size_t i = 0; i < n_tokens_; ++i) {
            if (selected[i]) out.token_ids.push_back(static_cast<std::uint32_t>(i));
        }

        cached_route_ = out;
        cached_step_ = step;
        cached_valid_ = true;
        return out;
    }

    std::size_t block_count() const {
        return head_dim_ ? summaries_.size() / head_dim_ : 0;
    }

private:
    Config cfg_;
    std::size_t head_dim_;
    const std::vector<float> * keys_ = nullptr;
    std::size_t n_tokens_ = 0;
    std::vector<float> summaries_;
    Route cached_route_;
    std::size_t cached_step_ = 0;
    bool cached_valid_ = false;

    void validate_keys(const std::vector<float> & keys, std::size_t n_tokens) const {
        if (keys.size() != n_tokens * head_dim_) {
            throw std::invalid_argument("key storage size mismatch");
        }
    }

    float norm(const float * x) const {
        float s=0.0f;
        for (std::size_t d=0; d<head_dim_; ++d) s += x[d]*x[d];
        return std::sqrt(s);
    }

    std::vector<float> pool_range(
        const std::vector<float> & keys, std::size_t begin_token, std::size_t end_token) const {
        if (begin_token >= end_token) return {};
        const std::size_t count = end_token - begin_token;
        const std::size_t blocks = (count + cfg_.block_size - 1) / cfg_.block_size;
        std::vector<float> out(blocks * head_dim_, 0.0f);
        for (std::size_t b=0; b<blocks; ++b) {
            const std::size_t begin = begin_token + b*cfg_.block_size;
            const std::size_t end = std::min(begin + cfg_.block_size, end_token);
            for (std::size_t t=begin; t<end; ++t) {
                for (std::size_t d=0; d<head_dim_; ++d) {
                    out[b*head_dim_+d] += keys[t*head_dim_+d];
                }
            }
            const float inv = 1.0f / static_cast<float>(end-begin);
            float sq=0.0f;
            for (std::size_t d=0; d<head_dim_; ++d) {
                out[b*head_dim_+d] *= inv;
                sq += out[b*head_dim_+d]*out[b*head_dim_+d];
            }
            const float inv_norm = 1.0f / std::max(std::sqrt(sq), 1e-12f);
            for (std::size_t d=0; d<head_dim_; ++d) out[b*head_dim_+d] *= inv_norm;
        }
        return out;
    }
};

} // namespace ahsma
