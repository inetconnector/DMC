cmake_minimum_required(VERSION 3.16)
project(dmc_reference LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dmc_demo
    main.cpp
)

add_executable(dmc_test
    test.cpp
)

target_include_directories(dmc_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(dmc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
