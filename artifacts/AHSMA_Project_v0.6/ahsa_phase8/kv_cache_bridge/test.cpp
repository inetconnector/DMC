#include "ahsma_kv_bridge.hpp"

#include <algorithm>
#include <cassert>
#include <set>
#include <vector>

using namespace ahsma;

static std::vector<float> make_keys(std::size_t n_tokens, std::size_t dim) {
    std::vector<float> out(n_tokens * dim, 0.0f);
    for (std::size_t i = 0; i < n_tokens; ++i) {
        out[i * dim + (i % dim)] = 1.0f;
    }
    return out;
}

static std::vector<TokenRef> make_refs(std::size_t n_tokens, seq_id_t seq) {
    std::vector<TokenRef> refs;
    refs.reserve(n_tokens);
    for (std::size_t i = 0; i < n_tokens; ++i) {
        refs.push_back(TokenRef{static_cast<std::uint32_t>(i), static_cast<pos_t>(i), seq});
    }
    return refs;
}

static std::vector<std::uint32_t> flatten_spans(const std::vector<Span> & spans) {
    std::vector<std::uint32_t> cells;
    for (const auto & span : spans) {
        for (std::uint32_t c = span.first_cell; c <= span.last_cell; ++c) {
            cells.push_back(c);
        }
    }
    return cells;
}

int main() {
    Config cfg;
    cfg.block_size = 8;
    cfg.route_dim = 4;
    cfg.local_window = 16;
    cfg.global_tokens = 4;
    cfg.retrieved_blocks = 2;
    cfg.route_refresh = 4;

    KVIndex idx(cfg);
    const auto refs = make_refs(64, 0);
    const auto keys = make_keys(refs.size(), cfg.route_dim);
    idx.rebuild(refs, keys);

    std::vector<float> q = {1.0f, 0.0f, 0.0f, 0.0f};
    auto route = idx.route(0, 63, q, 8);
    auto reused = idx.route(0, 63, q, 9);

    assert(!route.reused);
    assert(reused.reused);
    assert(!route.cells.empty());
    assert(!route.spans.empty());
    assert(flatten_spans(route.spans) == route.cells);

    const std::set<std::uint32_t> cell_set(route.cells.begin(), route.cells.end());
    assert(cell_set.count(0) == 1);
    assert(cell_set.count(63) == 1);

    const auto old_generation = idx.generation();
    idx.seq_rm(0, 8, 16);
    assert(idx.generation() > old_generation);

    auto after_rm = idx.route(0, 63, q, 12);
    assert(!after_rm.reused);
    assert(!after_rm.spans.empty());
    assert(flatten_spans(after_rm.spans) == after_rm.cells);

    idx.clear();
    assert(idx.block_count() == 0);

    return 0;
}
