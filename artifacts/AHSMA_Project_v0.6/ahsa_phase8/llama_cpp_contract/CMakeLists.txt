cmake_minimum_required(VERSION 3.16)
project(ahsma_llama_cpp_contract LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ahsma_llama_contract_bench
    benchmark.cpp
    ahsma_llama_contract.cpp)
add_executable(ahsma_llama_contract_test
    test.cpp
    ahsma_llama_contract.cpp)

target_include_directories(ahsma_llama_contract_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../kv_cache_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpu_selected_attention)
target_include_directories(ahsma_llama_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../kv_cache_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpu_selected_attention)

enable_testing()
add_test(NAME ahsma_llama_contract_test COMMAND $<TARGET_FILE:ahsma_llama_contract_test>)
