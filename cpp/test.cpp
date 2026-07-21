#include "dmc_reference.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

static std::vector<dmc::TokenRef> make_refs(std::size_t n_tokens, std::int32_t seq_id = 0) {
    std::vector<dmc::TokenRef> refs;
    refs.reserve(n_tokens);
    for (std::size_t i = 0; i < n_tokens; ++i) {
        refs.push_back(dmc::TokenRef{static_cast<std::uint32_t>(i), static_cast<std::int64_t>(i), seq_id});
    }
    return refs;
}

int main() {
    {
        std::vector<float> q = {1.f, 2.f, 3.f, 4.f};
        std::vector<float> k = {1.f, 0.f, 0.f, 1.f, 2.f, 1.f, 1.f, 2.f};
        std::vector<float> v = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f};
        auto dense = dmc::dense_attention(q, k, v, 2);
        auto exact = dmc::exact_attention(q, k, v, {0, 1, 2, 3}, 2);
        for (std::size_t i = 0; i < dense.size(); ++i) {
            assert(std::fabs(dense[i] - exact[i]) < 1e-5f);
        }
    }

    {
        dmc::DMCIndex index(dmc::DMCConfig{8, 16, 4, 3});
        index.rebuild(make_refs(64));
        auto selection = index.select(0, 47);
        assert(!selection.token_ids.empty());
        assert(selection.token_ids.back() < 48);
        assert(selection.spans.size() >= 2);
    }

    {
        std::vector<dmc::TokenRef> refs = {
            {0, 0, 0},
            {1, 0, 1},
            {2, 1, 0},
            {3, 1, 1},
        };
        dmc::DMCIndex index(dmc::DMCConfig{4, 8, 0, 0});
        index.rebuild(refs);
        auto sel0 = index.select(0, 1);
        auto sel1 = index.select(1, 1);
        assert(sel0.token_ids.size() == 2);
        assert(sel1.token_ids.size() == 2);
        assert(sel0.token_ids[0] == 0);
        assert(sel0.token_ids[1] == 2);
        assert(sel1.token_ids[0] == 1);
        assert(sel1.token_ids[1] == 3);
    }

    {
        dmc::DMCIndex index(dmc::DMCConfig{8, 8, 0, 3});
        index.rebuild(make_refs(128));
        const auto selection = index.select(0, 95);
        assert(selection.spans.size() == 4);
        assert(selection.spans[1].first_pos == 80 && selection.spans[1].last_pos == 87);
        assert(selection.spans[2].first_pos == 64 && selection.spans[2].last_pos == 79);
        assert(selection.spans[3].first_pos == 32 && selection.spans[3].last_pos == 63);
    }

    {
        const auto dense_plan = dmc::plan_runtime_context(
            128,
            4096,
            dmc::DMCConfig{8, 16, 4, 3});
        assert(!dense_plan.compacted);
        assert(dense_plan.token_ids.size() == 128);
        assert(dense_plan.token_ids.front() == 0);
        assert(dense_plan.token_ids.back() == 127);
    }

    {
        const dmc::DMCConfig cfg{64, 2048, 64, 4};
        const auto first = dmc::plan_runtime_context(131072, 4096, cfg);
        const auto second = dmc::plan_runtime_context(131072, 4096, cfg);
        assert(first.compacted);
        assert(first.token_ids == second.token_ids);
        assert(first.token_ids.size() <= 4096);
        assert(first.token_ids.front() == 0);
        assert(first.token_ids.back() == 131071);
        assert(std::is_sorted(first.token_ids.begin(), first.token_ids.end()));
        assert(first.spans.size() >= 3);
    }

    {
        const dmc::DMCConfig android_cfg{64, 2048, 64, 7};
        const auto plan = dmc::plan_runtime_context(131072, 12284, android_cfg);
        assert(plan.compacted);
        assert(plan.token_ids.size() == 6144);
        assert(plan.spans.size() == 8);
        assert(plan.spans[2].first_pos == 128960 && plan.spans[2].last_pos == 129023);
        assert(plan.spans[7].first_pos == 63488 && plan.spans[7].last_pos == 65535);
        assert(plan.token_ids.front() == 0);
        assert(plan.token_ids.back() == 131071);
    }

    std::cout << "dmc test passed\n";
    return 0;
}
