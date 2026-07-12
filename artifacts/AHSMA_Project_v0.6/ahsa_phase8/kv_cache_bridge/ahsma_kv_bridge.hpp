#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ahsma {

using seq_id_t = int32_t;
using pos_t = int64_t;

struct Config {
    std::uint32_t block_size = 64;
    std::uint32_t route_dim = 32;
    std::uint32_t local_window = 2048;
    std::uint32_t global_tokens = 64;
    std::uint32_t retrieved_blocks = 64;
    std::uint32_t route_refresh = 8;
};

struct TokenRef {
    std::uint32_t physical_cell = 0;
    pos_t logical_pos = 0;
    seq_id_t seq_id = -1;
};

struct Span {
    seq_id_t seq_id = -1;
    std::uint32_t first_cell = 0;
    std::uint32_t last_cell = 0;
    pos_t first_pos = 0;
    pos_t last_pos = 0;
};

struct Block {
    seq_id_t seq_id = -1;
    pos_t first_pos = 0;
    pos_t last_pos = 0;
    std::vector<std::uint32_t> cells;
    std::vector<float> summary;
};

struct Route {
    std::vector<std::uint32_t> cells;
    std::vector<std::uint32_t> block_ids;
    std::vector<Span> spans;
    bool reused = false;
};

class KVIndex {
public:
    explicit KVIndex(Config cfg) : cfg_(cfg) {
        if (cfg_.block_size == 0 || cfg_.route_dim == 0) {
            throw std::invalid_argument("invalid config");
        }
    }

    void rebuild(const std::vector<TokenRef> & refs, const std::vector<float> & route_keys) {
        validate_storage(refs, route_keys);
        refs_ = refs;
        route_keys_ = route_keys;
        generation_++;
        cache_.clear();
        rebuild_blocks();
    }

    void clear() {
        refs_.clear();
        route_keys_.clear();
        blocks_.clear();
        cell_lookup_.clear();
        cache_.clear();
        generation_++;
    }

    void seq_rm(seq_id_t seq, pos_t p0, pos_t p1) {
        mutate([&](const TokenRef & r) {
            return !(r.seq_id == seq && r.logical_pos >= p0 && (p1 < 0 || r.logical_pos < p1));
        });
    }

    void seq_keep(seq_id_t seq) {
        mutate([&](const TokenRef & r) { return r.seq_id == seq; });
    }

    void seq_cp(seq_id_t src, seq_id_t dst, pos_t p0, pos_t p1) {
        std::vector<TokenRef> extra;
        std::vector<float> extra_keys;
        for (std::size_t i = 0; i < refs_.size(); ++i) {
            const auto & r = refs_[i];
            if (r.seq_id == src && r.logical_pos >= p0 && (p1 < 0 || r.logical_pos < p1)) {
                auto copy = r;
                copy.seq_id = dst;
                extra.push_back(copy);
                extra_keys.insert(
                    extra_keys.end(),
                    route_keys_.begin() + static_cast<std::ptrdiff_t>(i * cfg_.route_dim),
                    route_keys_.begin() + static_cast<std::ptrdiff_t>((i + 1) * cfg_.route_dim));
            }
        }
        refs_.insert(refs_.end(), extra.begin(), extra.end());
        route_keys_.insert(route_keys_.end(), extra_keys.begin(), extra_keys.end());
        rebuild(refs_, route_keys_);
    }

    void seq_add(seq_id_t seq, pos_t p0, pos_t p1, pos_t shift) {
        for (auto & r : refs_) {
            if (r.seq_id == seq && r.logical_pos >= p0 && (p1 < 0 || r.logical_pos < p1)) {
                r.logical_pos += shift;
            }
        }
        rebuild(refs_, route_keys_);
    }

    void seq_div(seq_id_t seq, pos_t p0, pos_t p1, int d) {
        if (d == 0) {
            throw std::invalid_argument("division by zero");
        }
        for (auto & r : refs_) {
            if (r.seq_id == seq && r.logical_pos >= p0 && (p1 < 0 || r.logical_pos < p1)) {
                r.logical_pos /= d;
            }
        }
        rebuild(refs_, route_keys_);
    }

    Route route(seq_id_t seq, pos_t current_pos, const std::vector<float> & q, std::uint64_t step) {
        if (q.size() != cfg_.route_dim) {
            throw std::invalid_argument("query dimension mismatch");
        }

        auto & cached = cache_[seq];
        if (cached.valid &&
            cached.generation == generation_ &&
            step >= cached.step &&
            step - cached.step < cfg_.route_refresh) {
            auto out = cached.route;
            out.reused = true;
            return out;
        }

        const auto out = compute_route(seq, current_pos, q);
        cached.route = out;
        cached.step = step;
        cached.generation = generation_;
        cached.valid = true;
        return out;
    }

    std::vector<Span> selected_spans(
        seq_id_t seq,
        pos_t current_pos,
        const std::vector<float> & q,
        std::uint64_t step) {
        return route(seq, current_pos, q, step).spans;
    }

    std::size_t block_count() const {
        return blocks_.size();
    }

    std::uint64_t generation() const {
        return generation_;
    }

private:
    struct Cached {
        Route route;
        std::uint64_t step = 0;
        std::uint64_t generation = 0;
        bool valid = false;
    };

    Config cfg_;
    std::vector<TokenRef> refs_;
    std::vector<float> route_keys_;
    std::vector<Block> blocks_;
    std::unordered_map<std::uint32_t, TokenRef> cell_lookup_;
    std::map<seq_id_t, Cached> cache_;
    std::uint64_t generation_ = 0;

    void validate_storage(const std::vector<TokenRef> & refs, const std::vector<float> & keys) const {
        if (keys.size() != refs.size() * cfg_.route_dim) {
            throw std::invalid_argument("route key storage mismatch");
        }
    }

    void rebuild_blocks() {
        blocks_.clear();
        cell_lookup_.clear();

        std::map<seq_id_t, std::vector<std::size_t>> per_seq;
        for (std::size_t i = 0; i < refs_.size(); ++i) {
            const auto & ref = refs_[i];
            cell_lookup_[ref.physical_cell] = ref;
            per_seq[ref.seq_id].push_back(i);
        }

        for (auto & [seq, indices] : per_seq) {
            std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
                if (refs_[a].logical_pos != refs_[b].logical_pos) {
                    return refs_[a].logical_pos < refs_[b].logical_pos;
                }
                return refs_[a].physical_cell < refs_[b].physical_cell;
            });
            for (std::size_t i = 0; i < indices.size(); i += cfg_.block_size) {
                const std::size_t end = std::min(indices.size(), i + cfg_.block_size);
                append_block(seq, indices, i, end);
            }
        }
    }

    void append_block(seq_id_t seq, const std::vector<std::size_t> & indices, std::size_t begin, std::size_t end) {
        Block block;
        block.seq_id = seq;
        block.first_pos = refs_[indices[begin]].logical_pos;
        block.last_pos = refs_[indices[end - 1]].logical_pos;
        block.summary.assign(cfg_.route_dim, 0.0f);

        for (std::size_t j = begin; j < end; ++j) {
            const std::size_t idx = indices[j];
            const auto & ref = refs_[idx];
            block.cells.push_back(ref.physical_cell);
            const float * source = &route_keys_[idx * cfg_.route_dim];
            for (std::uint32_t d = 0; d < cfg_.route_dim; ++d) {
                block.summary[d] += source[d];
            }
        }

        float norm = 0.0f;
        for (float x : block.summary) {
            norm += x * x;
        }
        norm = std::sqrt(std::max(norm, 1e-20f));
        for (float & x : block.summary) {
            x /= norm;
        }

        blocks_.push_back(std::move(block));
    }

    template <class Pred>
    void mutate(Pred keep) {
        std::vector<TokenRef> new_refs;
        std::vector<float> new_keys;
        for (std::size_t i = 0; i < refs_.size(); ++i) {
            if (keep(refs_[i])) {
                new_refs.push_back(refs_[i]);
                new_keys.insert(
                    new_keys.end(),
                    route_keys_.begin() + static_cast<std::ptrdiff_t>(i * cfg_.route_dim),
                    route_keys_.begin() + static_cast<std::ptrdiff_t>((i + 1) * cfg_.route_dim));
            }
        }
        rebuild(new_refs, new_keys);
    }

    Route compute_route(seq_id_t seq, pos_t current_pos, const std::vector<float> & q) const {
        Route out;
        std::set<std::uint32_t> selected;
        std::vector<std::pair<float, std::uint32_t>> scored;

        const pos_t local_start = current_pos > static_cast<pos_t>(cfg_.local_window)
            ? current_pos - static_cast<pos_t>(cfg_.local_window)
            : 0;

        for (std::uint32_t bi = 0; bi < blocks_.size(); ++bi) {
            const auto & block = blocks_[bi];
            if (block.seq_id != seq) {
                continue;
            }
            if (block.first_pos > current_pos) {
                continue;
            }
            if (block.last_pos >= local_start) {
                selected.insert(block.cells.begin(), block.cells.end());
                continue;
            }

            float score = 0.0f;
            for (std::uint32_t d = 0; d < cfg_.route_dim; ++d) {
                score += q[d] * block.summary[d];
            }
            scored.emplace_back(score, bi);
        }

        const std::size_t keep = std::min<std::size_t>(cfg_.retrieved_blocks, scored.size());
        if (keep < scored.size()) {
            std::nth_element(
                scored.begin(),
                scored.begin() + static_cast<std::ptrdiff_t>(keep),
                scored.end(),
                [](const auto & a, const auto & b) { return a.first > b.first; });
            scored.resize(keep);
        }

        std::sort(scored.begin(), scored.end(), [](const auto & a, const auto & b) {
            return a.second < b.second;
        });

        for (const auto & [score, block_id] : scored) {
            (void)score;
            out.block_ids.push_back(block_id);
            selected.insert(blocks_[block_id].cells.begin(), blocks_[block_id].cells.end());
        }

        for (const auto & ref : refs_) {
            if (ref.seq_id == seq &&
                ref.logical_pos <= current_pos &&
                ref.logical_pos < static_cast<pos_t>(cfg_.global_tokens)) {
                selected.insert(ref.physical_cell);
            }
        }

        out.cells.assign(selected.begin(), selected.end());
        out.spans = build_spans(seq, out.cells);
        return out;
    }

    std::vector<Span> build_spans(seq_id_t seq, const std::vector<std::uint32_t> & cells) const {
        std::vector<Span> spans;
        if (cells.empty()) {
            return spans;
        }

        std::uint32_t start = cells.front();
        std::uint32_t previous = cells.front();
        const auto first_ref = cell_lookup_.at(start);

        auto flush = [&](std::uint32_t end_cell) {
            const auto last_ref = cell_lookup_.at(end_cell);
            spans.push_back(Span{
                seq,
                start,
                end_cell,
                first_ref.logical_pos,
                last_ref.logical_pos,
            });
        };

        for (std::size_t i = 1; i < cells.size(); ++i) {
            const std::uint32_t cell = cells[i];
            if (cell == previous + 1) {
                previous = cell;
                continue;
            }
            flush(previous);
            start = cell;
            previous = cell;
        }

        flush(previous);
        return spans;
    }
};

} // namespace ahsma
