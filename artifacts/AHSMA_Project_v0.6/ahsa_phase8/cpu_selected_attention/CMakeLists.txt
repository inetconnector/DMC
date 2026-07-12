cmake_minimum_required(VERSION 3.16)
project(ahsma_cpu_selected_attention LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ahsma_cpu_attention_bench benchmark.cpp)
add_executable(ahsma_cpu_attention_test test.cpp)

enable_testing()
add_test(NAME ahsma_cpu_attention_test COMMAND $<TARGET_FILE:ahsma_cpu_attention_test>)
