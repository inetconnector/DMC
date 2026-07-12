#include "ahsma_llama_contract.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace ahsma;

template <class Fn>
static std::pair<double, decltype(std::declval<Fn>()())> measure(Fn fn, std::size_t repeats) {
    using result_t = decltype(fn());
    for (int i = 0; i < 2; ++i) {
        (void)fn();
    }
    const auto start = std::chrono::steady_clock::now();
    result_t out = fn();
    for (std::size_t i = 1; i < repeats; ++i) {
        out = fn();
    }
    const auto end = std::chrono::steady_clock::now();
    const double total = std::chrono::duration<double>(end - start).count();
    return {total / static_cast<double>(repeats), out};
}

static std::vector<float> random_tensor(std::size_t count, std::mt19937 & rng) {
    std::normal_distribution<float> normal(0.0f, 1.0f);
    std::vector<float> out(count);
    for (float & x : out) {
        x = normal(rng);
    }
    return out;
}

int main(int argc, char ** argv) {
    const std::size_t tokens = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 4096;
    const std::size_t dim = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 32;
    const std::size_t repeats = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 10;

    LlamaAhsmaParams params;
    params.block_size = 64;
    params.route_dim = static_cast<std::uint32_t>(dim);
    params.local_window = 512;
    params.global_tokens = 64;
    params.retrieved_blocks = 32;
    params.route_refresh = 8;
    params.enabled = true;

    LlamaAhsmaContract contract(params);

    std::vector<TokenRef> refs;
    refs.reserve(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        refs.push_back(TokenRef{static_cast<std::uint32_t>(i), static_cast<pos_t>(i), 0});
    }

    std::mt19937 rng(19);
    auto route_keys = random_tensor(tokens * dim, rng);
    auto q = random_tensor(dim, rng);
    auto k = random_tensor(tokens * dim, rng);
    auto v = random_tensor(tokens * dim, rng);

    contract.rebuild(refs, route_keys);

    const auto [select_time, selection] = measure([&] {
        return contract.select(0, static_cast<pos_t>(tokens - 1), q, 8);
    }, repeats);

    LlamaAhsmaSelection full_selection;
    full_selection.token_ids.reserve(tokens);
    full_selection.spans.push_back(
        Span{0, 0, static_cast<std::uint32_t>(tokens - 1), 0, static_cast<pos_t>(tokens - 1)});
    for (std::size_t i = 0; i < tokens; ++i) {
        full_selection.token_ids.push_back(static_cast<std::uint32_t>(i));
    }

    const auto [dense_time, dense] = measure([&] {
        return contract.attend_dense(q, k, v, dim);
    }, repeats);
    const auto [selected_time, selected] = measure([&] {
        return contract.attend_selected(q, k, v, dim, full_selection);
    }, repeats);
    const auto [span_time, span_selected] = measure([&] {
        return contract.attend_selected_from_spans(q, k, v, dim, full_selection);
    }, repeats);

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"tokens\": " << tokens << ",\n"
              << "  \"head_dim\": " << dim << ",\n"
              << "  \"selection_ms\": " << select_time * 1000.0 << ",\n"
              << "  \"dense_ms\": " << dense_time * 1000.0 << ",\n"
              << "  \"selected_ms\": " << selected_time * 1000.0 << ",\n"
              << "  \"span_selected_ms\": " << span_time * 1000.0 << ",\n"
              << "  \"dense_vs_selected_max_abs_diff\": " << max_abs_diff(dense, selected) << ",\n"
              << "  \"selected_vs_span_max_abs_diff\": " << max_abs_diff(selected, span_selected) << ",\n"
              << "  \"selected_tokens\": " << selection.token_ids.size() << ",\n"
              << "  \"selected_spans\": " << selection.spans.size() << ",\n"
              << "  \"oracle_tokens\": " << full_selection.token_ids.size() << ",\n"
              << "  \"oracle_spans\": " << full_selection.spans.size() << "\n"
              << "}\n";
}
