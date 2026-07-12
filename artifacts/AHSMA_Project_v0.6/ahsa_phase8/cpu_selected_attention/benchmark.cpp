#include "ahsma_cpu_attention.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <utility>
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

static std::vector<float> make_random(std::size_t count, std::mt19937 & rng) {
    std::normal_distribution<float> normal(0.0f, 1.0f);
    std::vector<float> out(count);
    for (float & x : out) {
        x = normal(rng);
    }
    return out;
}

int main(int argc, char ** argv) {
    const std::size_t tokens = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 4096;
    const std::size_t head_dim = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 64;
    const std::size_t queries = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1;
    const std::size_t repeats = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 10;

    std::mt19937 rng(11);
    auto q = make_random(queries * head_dim, rng);
    auto k = make_random(tokens * head_dim, rng);
    auto v = make_random(tokens * head_dim, rng);

    std::vector<std::uint32_t> all_ids(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        all_ids[i] = static_cast<std::uint32_t>(i);
    }

    std::vector<CellSpan> selected_spans;
    const std::size_t local_start = tokens > 512 ? tokens - 512 : 0;
    selected_spans.push_back(
        CellSpan{0, static_cast<std::uint32_t>(std::min<std::size_t>(63, tokens - 1))});
    if (local_start < tokens) {
        selected_spans.push_back(
            CellSpan{static_cast<std::uint32_t>(local_start), static_cast<std::uint32_t>(tokens - 1)});
    }

    const auto [dense_time, dense_out] = measure([&] {
        return dense_attention(q, k, v, head_dim);
    }, repeats);

    const auto [full_time, full_out] = measure([&] {
        return exact_selected_attention(q, k, v, all_ids, head_dim).output;
    }, repeats);

    const auto [span_time, span_out] = measure([&] {
        return exact_selected_attention_from_spans(q, k, v, selected_spans, head_dim).output;
    }, repeats);

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"tokens\": " << tokens << ",\n"
              << "  \"head_dim\": " << head_dim << ",\n"
              << "  \"queries\": " << queries << ",\n"
              << "  \"dense_ms\": " << dense_time * 1000.0 << ",\n"
              << "  \"full_selected_ms\": " << full_time * 1000.0 << ",\n"
              << "  \"span_selected_ms\": " << span_time * 1000.0 << ",\n"
              << "  \"full_selection_max_abs_diff\": " << max_abs_diff(dense_out, full_out) << ",\n"
              << "  \"span_vs_full_max_abs_diff\": " << max_abs_diff(full_out, span_out) << "\n"
              << "}\n";
}
