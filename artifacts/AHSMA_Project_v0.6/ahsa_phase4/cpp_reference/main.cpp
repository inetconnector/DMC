#include "ahsma_index.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

int main(int argc, char ** argv) {
    const std::size_t tokens = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 32768;
    const std::size_t dim    = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 128;
    const std::size_t reps   = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 100;

    ahsma::Config cfg;
    cfg.block_size=64; cfg.local_window=2048; cfg.global_tokens=64;
    cfg.retrieved_blocks=64; cfg.route_refresh=8;

    std::mt19937 rng(7);
    std::normal_distribution<float> normal(0.0f,1.0f);
    std::vector<float> keys(tokens*dim), query(dim);
    for (float & x: keys) x=normal(rng);
    for (float & x: query) x=normal(rng);

    ahsma::BlockIndex index(cfg,dim);
    auto t0=std::chrono::steady_clock::now();
    index.build(keys,tokens);
    auto t1=std::chrono::steady_clock::now();

    std::size_t total_ids=0;
    auto t2=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<reps;++i) {
        auto route=index.route(query,i*cfg.route_refresh);
        total_ids += route.token_ids.size();
    }
    auto t3=std::chrono::steady_clock::now();

    auto cached=index.route(query,(reps-1)*cfg.route_refresh+1);
    const double build_ms=std::chrono::duration<double,std::milli>(t1-t0).count();
    const double route_us=std::chrono::duration<double,std::micro>(t3-t2).count()/reps;

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"tokens\": " << tokens << ",\n"
              << "  \"head_dim\": " << dim << ",\n"
              << "  \"blocks\": " << index.block_count() << ",\n"
              << "  \"index_build_ms\": " << build_ms << ",\n"
              << "  \"fresh_route_us\": " << route_us << ",\n"
              << "  \"mean_active_tokens\": " << (total_ids/reps) << ",\n"
              << "  \"cached_route_reused\": " << (cached.reused ? "true":"false") << "\n"
              << "}\n";
}
