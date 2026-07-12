#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ahsma {

using seq_id_t = int32_t;
using pos_t = int64_t;

struct Config {
    uint32_t block_size = 64;
    uint32_t route_dim = 32;
    uint32_t local_window = 2048;
    uint32_t global_tokens = 64;
    uint32_t retrieved_blocks = 64;
    uint32_t route_refresh = 8;
};

struct TokenRef {
    uint32_t physical_cell;
    pos_t logical_pos;
    seq_id_t seq_id;
};

struct Block {
    seq_id_t seq_id = -1;
    pos_t first_pos = 0;
    pos_t last_pos = 0;
    std::vector<uint32_t> cells;
    std::vector<float> summary;
    uint64_t generation = 0;
};

struct Route {
    std::vector<uint32_t> cells;
    std::vector<uint32_t> block_ids;
    bool reused = false;
};

class KVIndex {
public:
    explicit KVIndex(Config cfg) : cfg_(cfg) {
        if (!cfg_.block_size || !cfg_.route_dim) {
            throw std::invalid_argument("invalid config");
        }
    }

    void rebuild(const std::vector<TokenRef> & refs, const std::vector<float> & route_keys) {
        validate_storage(refs, route_keys);
        refs_ = refs;
        route_keys_ = route_keys;
        blocks_.clear();
        cache_.clear();

        std::map<seq_id_t, std::vector<TokenRef>> per_seq;
        for (const auto & r : refs_) per_seq[r.seq_id].push_back(r);
        for (auto & [seq, items] : per_seq) {
            std::sort(items.begin(), items.end(), [](const auto & a, const auto & b) {
                return a.logical_pos < b.logical_pos;
            });
            for (size_t i = 0; i < items.size(); i += cfg_.block_size) {
                const size_t end = std::min(items.size(), i + cfg_.block_size);
                append_block(seq, items, i, end);
            }
        }
        generation_++;
    }

    void clear() {
        refs_.clear();
        route_keys_.clear();
        blocks_.clear();
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
        for (size_t i = 0; i < refs_.size(); ++i) {
            const auto & r = refs_[i];
            if (r.seq_id == src && r.logical_pos >= p0 && (p1 < 0 || r.logical_pos < p1)) {
                auto c = r;
                c.seq_id = dst;
                extra.push_back(c);
                extra_keys.insert(extra_keys.end(),
                    route_keys_.begin() + i*cfg_.route_dim,
                    route_keys_.begin() + (i+1)*cfg_.route_dim);
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
        if (d == 0) throw std::invalid_argument("division by zero");
        for (auto & r : refs_) {
            if (r.seq_id == seq && r.logical_pos >= p0 && (p1 < 0 || r.logical_pos < p1)) {
                r.logical_pos /= d;
            }
        }
        rebuild(refs_, route_keys_);
    }

    Route route(seq_id_t seq, pos_t current_pos, const std::vector<float> & q, uint64_t step) {
        if (q.size() != cfg_.route_dim) throw std::invalid_argument("query dimension mismatch");
        auto & c = cache_[seq];
        if (c.valid && c.generation == generation_ && step >= c.step &&
            step - c.step < cfg_.route_refresh) {
            auto out = c.route;
            out.reused = true;
            return out;
        }

        std::vector<std::pair<float,uint32_t>> scored;
        std::set<uint32_t> selected;

        for (uint32_t bi = 0; bi < blocks_.size(); ++bi) {
            const auto & b = blocks_[bi];
            if (b.seq_id != seq || b.first_pos > current_pos) continue;
            if (b.last_pos >= current_pos - static_cast<pos_t>(cfg_.local_window)) {
                selected.insert(b.cells.begin(), b.cells.end());
                continue;
            }
            float s = 0.0f;
            for (uint32_t d = 0; d < cfg_.route_dim; ++d) s += q[d] * b.summary[d];
            scored.emplace_back(s, bi);
        }

        const size_t keep = std::min<size_t>(cfg_.retrieved_blocks, scored.size());
        if (keep < scored.size()) {
            std::nth_element(scored.begin(), scored.begin()+keep, scored.end(),
                [](auto & a, auto & b){ return a.first > b.first; });
            scored.resize(keep);
        }

        Route out;
        for (auto & [_, bi] : scored) {
            out.block_ids.push_back(bi);
            selected.insert(blocks_[bi].cells.begin(), blocks_[bi].cells.end());
        }

        // Global prefix safety path by logical position.
        for (const auto & r : refs_) {
            if (r.seq_id == seq && r.logical_pos < static_cast<pos_t>(cfg_.global_tokens)) {
                selected.insert(r.physical_cell);
            }
        }
        out.cells.assign(selected.begin(), selected.end());

        c.route = out;
        c.step = step;
        c.generation = generation_;
        c.valid = true;
        return out;
    }

    size_t block_count() const { return blocks_.size(); }
    uint64_t generation() const { return generation_; }

private:
    struct Cached {
        Route route;
        uint64_t step = 0;
        uint64_t generation = 0;
        bool valid = false;
    };

    Config cfg_;
    std::vector<TokenRef> refs_;
    std::vector<float> route_keys_;
    std::vector<Block> blocks_;
    std::map<seq_id_t,Cached> cache_;
    uint64_t generation_ = 0;

    void validate_storage(const std::vector<TokenRef> & refs, const std::vector<float> & keys) const {
        if (keys.size() != refs.size()*cfg_.route_dim) {
            throw std::invalid_argument("route key storage mismatch");
        }
    }

    void append_block(seq_id_t seq, const std::vector<TokenRef> & items, size_t begin, size_t end) {
        Block b;
        b.seq_id = seq;
        b.first_pos = items[begin].logical_pos;
        b.last_pos = items[end-1].logical_pos;
        b.summary.assign(cfg_.route_dim, 0.0f);
        for (size_t j = begin; j < end; ++j) {
            b.cells.push_back(items[j].physical_cell);
            auto it = std::find_if(refs_.begin(), refs_.end(), [&](const auto & r) {
                return r.physical_cell == items[j].physical_cell &&
                       r.logical_pos == items[j].logical_pos &&
                       r.seq_id == items[j].seq_id;
            });
            const size_t idx = std::distance(refs_.begin(), it);
            for (uint32_t d = 0; d < cfg_.route_dim; ++d) {
                b.summary[d] += route_keys_[idx*cfg_.route_dim+d];
            }
        }
        float n = 0.0f;
        for (float x : b.summary) n += x*x;
        n = std::sqrt(std::max(n, 1e-20f));
        for (float & x : b.summary) x /= n;
        b.generation = generation_;
        blocks_.push_back(std::move(b));
    }

    template<class Pred>
    void mutate(Pred keep) {
        std::vector<TokenRef> nr;
        std::vector<float> nk;
        for (size_t i = 0; i < refs_.size(); ++i) {
            if (keep(refs_[i])) {
                nr.push_back(refs_[i]);
                nk.insert(nk.end(),
                    route_keys_.begin()+i*cfg_.route_dim,
                    route_keys_.begin()+(i+1)*cfg_.route_dim);
            }
        }
        rebuild(nr, nk);
    }
};

} // namespace ahsma
