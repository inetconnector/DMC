#include "ahsma_llama_gpu_contract.hpp"

#include "../cpu_selected_attention/ahsma_cpu_attention.hpp"

#include <cuda_runtime.h>

#include <chrono>
#include <cstdlib>
#include <cstdint>
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

static std::vector<TokenRef> make_refs(std::size_t tokens) {
    std::vector<TokenRef> refs;
    refs.reserve(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        refs.push_back(TokenRef{static_cast<std::uint32_t>(i), static_cast<pos_t>(i), 0});
    }
    return refs;
}

static std::vector<std::uint32_t> make_all_ids(std::size_t tokens) {
    std::vector<std::uint32_t> out(tokens);
    for (std::size_t i = 0; i < tokens; ++i) {
        out[i] = static_cast<std::uint32_t>(i);
    }
    return out;
}

int main(int argc, char ** argv) {
    const std::size_t tokens = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 32768;
    const std::size_t head_dim = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 64;
    const std::size_t route_dim = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 32;
    const std::size_t queries = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 128;
    const std::size_t repeats = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 5;

    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess || device_count <= 0) {
        std::cerr << "CUDA device unavailable: " << cudaGetErrorString(err) << "\n";
        return 1;
    }

    cudaDeviceProp prop{};
    err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess) {
        std::cerr << "cudaGetDeviceProperties failed: " << cudaGetErrorString(err) << "\n";
        return 1;
    }

    size_t free_mem = 0;
    size_t total_mem = 0;
    err = cudaMemGetInfo(&free_mem, &total_mem);
    if (err != cudaSuccess) {
        std::cerr << "cudaMemGetInfo failed: " << cudaGetErrorString(err) << "\n";
        return 1;
    }

    LlamaAhsmaParams params;
    params.block_size = 64;
    params.route_dim = static_cast<std::uint32_t>(route_dim);
    params.local_window = 512;
    params.global_tokens = 64;
    params.retrieved_blocks = 32;
    params.route_refresh = 8;
    params.enabled = true;

    LlamaAhsmaGpuContract contract(params);

    std::mt19937 rng(31);
    const auto refs = make_refs(tokens);
    const auto route_keys = make_random(tokens * route_dim, rng);
    auto q = make_random(queries * head_dim, rng);
    auto k = make_random(tokens * head_dim, rng);
    auto v = make_random(tokens * head_dim, rng);

    const auto [build_ms, _ignored] = measure([&] {
        contract.rebuild(refs, route_keys);
        return 0;
    }, 1);

    std::vector<float> route_query(route_dim, 0.0f);
    for (std::size_t i = 0; i < route_dim; ++i) {
        route_query[i] = route_keys[i];
    }

    const auto [select_ms, selection] = measure([&] {
        return contract.select(0, static_cast<pos_t>(tokens - 1), route_query, 8);
    }, repeats);

    const auto [dense_ms, dense_out] = measure([&] {
        return contract.attend_dense(q, k, v, head_dim);
    }, repeats);

    const auto [cpu_ms, cpu_out] = measure([&] {
        return contract.attend_selected_cpu(q, k, v, head_dim, selection);
    }, repeats);

    const auto [gpu_ms, gpu_out] = measure([&] {
        return run_selected_attention_cuda(q, k, v, selection.token_ids, head_dim);
    }, repeats);

    const auto [gpu_span_ms, gpu_span_out] = measure([&] {
        auto spans = selection.spans;
        std::vector<std::uint32_t> ids;
        for (const auto & span : spans) {
            for (std::uint32_t i = span.first_cell; i <= span.last_cell; ++i) {
                ids.push_back(i);
            }
        }
        return run_selected_attention_cuda(q, k, v, ids, head_dim);
    }, repeats);

    const auto all_ids = make_all_ids(tokens);
    const auto dense_vs_gpu = max_abs_diff(
        dense_out,
        run_selected_attention_cuda(q, k, v, all_ids, head_dim).output);

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"device_name\": \"" << prop.name << "\",\n"
              << "  \"device_sm\": \"" << prop.major << "." << prop.minor << "\",\n"
              << "  \"device_total_mem_mb\": " << static_cast<std::uint64_t>(total_mem / (1024 * 1024)) << ",\n"
              << "  \"device_free_mem_mb\": " << static_cast<std::uint64_t>(free_mem / (1024 * 1024)) << ",\n"
              << "  \"tokens\": " << tokens << ",\n"
              << "  \"head_dim\": " << head_dim << ",\n"
              << "  \"route_dim\": " << route_dim << ",\n"
              << "  \"queries\": " << queries << ",\n"
              << "  \"blocks\": " << contract.block_count() << ",\n"
              << "  \"index_build_ms\": " << build_ms * 1000.0 << ",\n"
              << "  \"route_select_ms\": " << select_ms * 1000.0 << ",\n"
              << "  \"selected_cells\": " << selection.token_ids.size() << ",\n"
              << "  \"selected_spans\": " << selection.spans.size() << ",\n"
              << "  \"dense_ms\": " << dense_ms * 1000.0 << ",\n"
              << "  \"cpu_selected_ms\": " << cpu_ms * 1000.0 << ",\n"
              << "  \"gpu_selected_ms\": " << gpu_ms * 1000.0 << ",\n"
              << "  \"gpu_selected_kernel_ms\": " << gpu_out.kernel_ms << ",\n"
              << "  \"gpu_span_ms\": " << gpu_span_ms * 1000.0 << ",\n"
              << "  \"cpu_vs_gpu_max_abs_diff\": " << max_abs_diff(cpu_out, gpu_out.output) << ",\n"
              << "  \"gpu_vs_span_max_abs_diff\": " << max_abs_diff(gpu_out.output, gpu_span_out.output) << ",\n"
              << "  \"dense_vs_gpu_full_max_abs_diff\": " << dense_vs_gpu << "\n"
              << "}\n";
}
