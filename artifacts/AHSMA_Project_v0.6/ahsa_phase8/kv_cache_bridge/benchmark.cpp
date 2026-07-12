#include "ahsma_kv_bridge.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace ahsma;

static void sync_clock() {}

template <class Fn>
static std::pair<double, decltype(std::declval<Fn>()())> measure(Fn fn, std::size_t repeats) {
    using result_t = decltype(fn());
    for (int i = 0; i < 2; ++i) {
        (void)fn();
    }
    sync_clock();
    const auto start = std::chrono::steady_clock::now();
    result_t out = fn();
    for (std::size_t i = 1; i < repeats; ++i) {
        out = fn();
    }
    sync_clock();
    const auto end = std::chrono::steady_clock::now();
    const double total = std::chrono::duration<double>(end - start).count();
    return {total / static_cast<double>(repeats), out};
}

int main(int argc, char ** argv) {
    const std::size_t tokens = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 32768;
    const std::size_t dim = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 32;
    const std::size_t repeats = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 25;

    Config cfg;
    cfg.block_size = 64;
    cfg.route_dim = static_cast<std::uint32_t>(dim);
    cfg.local_window = 2048;
    cfg.global_tokens = 64;
    cfg.retrieved_blocks = 64;
    cfg.route_refresh = 8;

    std::mt19937 rng(7);
    std::normal_distribution<float> normal(0.0f, 1.0f);

    std::vector<TokenRef> refs;
    refs.reserve(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        refs.push_back(TokenRef{static_cast<std::uint32_t>(i), static_cast<pos_t>(i), 0});
    }

    std::vector<float> route_keys(tokens * dim);
    for (float & x : route_keys) {
        x = normal(rng);
    }

    std::vector<float> query(dim);
    for (float & x : query) {
        x = normal(rng);
    }

    KVIndex idx(cfg);
    const auto [build_time, _ignored] = measure([&] {
        idx.rebuild(refs, route_keys);
        return 0;
    }, 1);

    const auto [fresh_time, route] = measure([&] {
        return idx.route(0, static_cast<pos_t>(tokens - 1), query, 8);
    }, repeats);

    const auto [reuse_time, reused] = measure([&] {
        return idx.route(0, static_cast<pos_t>(tokens - 1), query, 9);
    }, repeats);

    const std::size_t selected_cells = route.cells.size();
    const std::size_t selected_spans = route.spans.size();

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"tokens\": " << tokens << ",\n"
              << "  \"route_dim\": " << dim << ",\n"
              << "  \"blocks\": " << idx.block_count() << ",\n"
              << "  \"index_build_ms\": " << build_time * 1000.0 << ",\n"
              << "  \"fresh_route_us\": " << fresh_time * 1000000.0 << ",\n"
              << "  \"reused_route_us\": " << reuse_time * 1000000.0 << ",\n"
              << "  \"selected_cells\": " << selected_cells << ",\n"
              << "  \"selected_spans\": " << selected_spans << ",\n"
              << "  \"cached_route_reused\": " << (reused.reused ? "true" : "false") << "\n"
              << "}\n";
}
