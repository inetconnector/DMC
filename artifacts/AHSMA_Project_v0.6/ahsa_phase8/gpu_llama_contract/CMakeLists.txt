cmake_minimum_required(VERSION 3.18)
project(ahsma_gpu_llama_contract LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_ARCHITECTURES 86)

find_package(CUDAToolkit REQUIRED)

add_executable(ahsma_gpu_llama_contract_test
    test.cpp
    ahsma_llama_gpu_contract.cpp
    ../gpu_selected_attention/ahsma_gpu_attention.cu)
target_include_directories(ahsma_gpu_llama_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../llama_cpp_contract
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpu_selected_attention
    ${CMAKE_CURRENT_SOURCE_DIR}/../kv_cache_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu_selected_attention)
target_link_libraries(ahsma_gpu_llama_contract_test PRIVATE CUDA::cudart)

add_executable(ahsma_gpu_llama_contract_bench
    benchmark.cpp
    ahsma_llama_gpu_contract.cpp
    ../gpu_selected_attention/ahsma_gpu_attention.cu)
target_include_directories(ahsma_gpu_llama_contract_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../llama_cpp_contract
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpu_selected_attention
    ${CMAKE_CURRENT_SOURCE_DIR}/../kv_cache_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu_selected_attention)
target_link_libraries(ahsma_gpu_llama_contract_bench PRIVATE CUDA::cudart)

enable_testing()
add_test(NAME ahsma_gpu_llama_contract_test COMMAND $<TARGET_FILE:ahsma_gpu_llama_contract_test>)
