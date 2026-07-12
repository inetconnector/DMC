#include "ahsma_gpu_attention.cuh"

#include <cuda_runtime.h>

#include <cfloat>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace ahsma {
namespace {

constexpr int kBlockSize = 256;

inline void cuda_check(cudaError_t err, const char * what) {
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(err));
    }
}

template <int BLOCK_SIZE>
__device__ float block_reduce_max(float val) {
    __shared__ float shared[BLOCK_SIZE];
    const int tid = threadIdx.x;
    shared[tid] = val;
    __syncthreads();
    for (int stride = BLOCK_SIZE / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared[tid] = fmaxf(shared[tid], shared[tid + stride]);
        }
        __syncthreads();
    }
    return shared[0];
}

template <int BLOCK_SIZE>
__device__ float block_reduce_sum(float val) {
    __shared__ float shared[BLOCK_SIZE];
    const int tid = threadIdx.x;
    shared[tid] = val;
    __syncthreads();
    for (int stride = BLOCK_SIZE / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared[tid] += shared[tid + stride];
        }
        __syncthreads();
    }
    return shared[0];
}

template <int BLOCK_SIZE>
__global__ void selected_attention_kernel(
    const float * q,
    const float * k,
    const float * v,
    const std::uint32_t * token_ids,
    int selected_count,
    int head_dim,
    int queries,
    float * out,
    float * scores) {
    const int qid = blockIdx.x;
    if (qid >= queries) {
        return;
    }

    const float * qvec = q + static_cast<std::size_t>(qid) * head_dim;
    float local_max = -FLT_MAX;
    for (int idx = threadIdx.x; idx < selected_count; idx += BLOCK_SIZE) {
        const std::uint32_t tid = token_ids[idx];
        const float * kvec = k + static_cast<std::size_t>(tid) * head_dim;
        float score = 0.0f;
        for (int d = 0; d < head_dim; ++d) {
            score += qvec[d] * kvec[d];
        }
        score /= sqrtf(static_cast<float>(head_dim));
        scores[static_cast<std::size_t>(qid) * selected_count + idx] = score;
        local_max = fmaxf(local_max, score);
    }

    const float max_score = block_reduce_max<BLOCK_SIZE>(local_max);
    __syncthreads();

    float local_sum = 0.0f;
    for (int idx = threadIdx.x; idx < selected_count; idx += BLOCK_SIZE) {
        local_sum += expf(scores[static_cast<std::size_t>(qid) * selected_count + idx] - max_score);
    }

    const float sum_score = block_reduce_sum<BLOCK_SIZE>(local_sum);
    __syncthreads();

    for (int idx = threadIdx.x; idx < selected_count; idx += BLOCK_SIZE) {
        const float prob =
            expf(scores[static_cast<std::size_t>(qid) * selected_count + idx] - max_score) / sum_score;
        scores[static_cast<std::size_t>(qid) * selected_count + idx] = prob;
    }
    __syncthreads();

    for (int d = threadIdx.x; d < head_dim; d += BLOCK_SIZE) {
        float acc = 0.0f;
        for (int idx = 0; idx < selected_count; ++idx) {
            const std::uint32_t tid = token_ids[idx];
            acc += scores[static_cast<std::size_t>(qid) * selected_count + idx] *
                   v[static_cast<std::size_t>(tid) * head_dim + d];
        }
        out[static_cast<std::size_t>(qid) * head_dim + d] = acc;
    }
}

} // namespace

CudaAttentionResult run_selected_attention_cuda(
    const std::vector<float> & q,
    const std::vector<float> & k,
    const std::vector<float> & v,
    const std::vector<std::uint32_t> & token_ids,
    std::size_t head_dim) {
    if (head_dim == 0) {
        throw std::invalid_argument("head_dim must be positive");
    }
    if (q.size() % head_dim != 0) {
        throw std::invalid_argument("q shape mismatch");
    }
    if (k.size() != v.size() || k.size() % head_dim != 0) {
        throw std::invalid_argument("k/v shape mismatch");
    }
    if (token_ids.empty()) {
        throw std::invalid_argument("token_ids must not be empty");
    }

    const int queries = static_cast<int>(q.size() / head_dim);
    const int selected_count = static_cast<int>(token_ids.size());

    float * d_q = nullptr;
    float * d_k = nullptr;
    float * d_v = nullptr;
    std::uint32_t * d_ids = nullptr;
    float * d_out = nullptr;
    float * d_scores = nullptr;

    cuda_check(cudaMalloc(reinterpret_cast<void **>(&d_q), q.size() * sizeof(float)), "cudaMalloc q");
    cuda_check(cudaMalloc(reinterpret_cast<void **>(&d_k), k.size() * sizeof(float)), "cudaMalloc k");
    cuda_check(cudaMalloc(reinterpret_cast<void **>(&d_v), v.size() * sizeof(float)), "cudaMalloc v");
    cuda_check(cudaMalloc(reinterpret_cast<void **>(&d_ids), token_ids.size() * sizeof(std::uint32_t)), "cudaMalloc ids");
    cuda_check(cudaMalloc(reinterpret_cast<void **>(&d_out), q.size() * sizeof(float)), "cudaMalloc out");
    cuda_check(cudaMalloc(reinterpret_cast<void **>(&d_scores), q.size() / head_dim * token_ids.size() * sizeof(float)), "cudaMalloc scores");

    cuda_check(cudaMemcpy(d_q, q.data(), q.size() * sizeof(float), cudaMemcpyHostToDevice), "cudaMemcpy q");
    cuda_check(cudaMemcpy(d_k, k.data(), k.size() * sizeof(float), cudaMemcpyHostToDevice), "cudaMemcpy k");
    cuda_check(cudaMemcpy(d_v, v.data(), v.size() * sizeof(float), cudaMemcpyHostToDevice), "cudaMemcpy v");
    cuda_check(cudaMemcpy(d_ids, token_ids.data(), token_ids.size() * sizeof(std::uint32_t), cudaMemcpyHostToDevice), "cudaMemcpy ids");

    cudaEvent_t start = nullptr;
    cudaEvent_t stop = nullptr;
    cuda_check(cudaEventCreate(&start), "cudaEventCreate start");
    cuda_check(cudaEventCreate(&stop), "cudaEventCreate stop");

    cuda_check(cudaEventRecord(start), "cudaEventRecord start");
    selected_attention_kernel<kBlockSize><<<queries, kBlockSize>>>(
        d_q,
        d_k,
        d_v,
        d_ids,
        selected_count,
        static_cast<int>(head_dim),
        queries,
        d_out,
        d_scores);
    cuda_check(cudaGetLastError(), "kernel launch");
    cuda_check(cudaEventRecord(stop), "cudaEventRecord stop");
    cuda_check(cudaEventSynchronize(stop), "cudaEventSynchronize stop");

    float kernel_ms = 0.0f;
    cuda_check(cudaEventElapsedTime(&kernel_ms, start, stop), "cudaEventElapsedTime");

    CudaAttentionResult result;
    result.output.resize(q.size());
    result.kernel_ms = kernel_ms;
    cuda_check(cudaMemcpy(result.output.data(), d_out, q.size() * sizeof(float), cudaMemcpyDeviceToHost), "cudaMemcpy out");

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaFree(d_scores);
    cudaFree(d_out);
    cudaFree(d_ids);
    cudaFree(d_v);
    cudaFree(d_k);
    cudaFree(d_q);

    return result;
}

} // namespace ahsma
