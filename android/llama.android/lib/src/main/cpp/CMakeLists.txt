cmake_minimum_required(VERSION 3.31.6)

project("ai-chat" VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED true)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}"   CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)

# --------------------------------------------------------------------------
# AI Chat library
# --------------------------------------------------------------------------

if(DEFINED ANDROID_ABI)
    message(STATUS "Detected Android ABI: ${ANDROID_ABI}")
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(GGML_SYSTEM_ARCH "ARM")
        set(GGML_CPU_KLEIDIAI OFF)
        set(GGML_OPENMP ON)
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(GGML_SYSTEM_ARCH "x86")
        set(GGML_CPU_KLEIDIAI OFF)
        set(GGML_OPENMP OFF)
    else()
        message(FATAL_ERROR "Unsupported ABI: ${ANDROID_ABI}")
    endif()
endif()

option(AI_CHAT_PREFER_OPENCL "Prefer the OpenCL backend when build prerequisites are available" ON)
option(AI_CHAT_PREFER_VULKAN "Prefer the Vulkan backend when build prerequisites are available" ON)

set(_AI_CHAT_VULKAN_SDK "")
if (DEFINED ENV{VULKAN_SDK})
    file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" _AI_CHAT_VULKAN_SDK)
endif()

set(_AI_CHAT_VULKAN_SHADER_TOOLCHAIN "")
if (DEFINED GGML_VULKAN_SHADERS_GEN_TOOLCHAIN AND GGML_VULKAN_SHADERS_GEN_TOOLCHAIN)
    file(TO_CMAKE_PATH "${GGML_VULKAN_SHADERS_GEN_TOOLCHAIN}" _AI_CHAT_VULKAN_SHADER_TOOLCHAIN)
    if (NOT EXISTS "${_AI_CHAT_VULKAN_SHADER_TOOLCHAIN}")
        message(WARNING "GGML_VULKAN_SHADERS_GEN_TOOLCHAIN points to a missing file: ${_AI_CHAT_VULKAN_SHADER_TOOLCHAIN}")
        set(_AI_CHAT_VULKAN_SHADER_TOOLCHAIN "")
    endif()
endif()

set(_AI_CHAT_HOST_CL "")
if (WIN32 AND NOT _AI_CHAT_VULKAN_SHADER_TOOLCHAIN)
    find_program(_AI_CHAT_HOST_CL NAMES cl.exe cl NO_CMAKE_FIND_ROOT_PATH)
endif()

if (AI_CHAT_PREFER_VULKAN AND _AI_CHAT_VULKAN_SDK AND EXISTS "${_AI_CHAT_VULKAN_SDK}/Bin/glslc.exe")
    if (_AI_CHAT_VULKAN_SHADER_TOOLCHAIN OR _AI_CHAT_HOST_CL)
        message(STATUS "Vulkan SDK detected at ${_AI_CHAT_VULKAN_SDK}; enabling ggml-vulkan")
        if (_AI_CHAT_VULKAN_SHADER_TOOLCHAIN)
            message(STATUS "Using explicit vulkan-shaders-gen toolchain: ${_AI_CHAT_VULKAN_SHADER_TOOLCHAIN}")
            set(GGML_VULKAN_SHADERS_GEN_TOOLCHAIN "${_AI_CHAT_VULKAN_SHADER_TOOLCHAIN}" CACHE FILEPATH "ggml: toolchain file for vulkan-shaders-gen" FORCE)
        else()
            message(STATUS "Host compiler detected for vulkan-shaders-gen: ${_AI_CHAT_HOST_CL}")
        endif()
        set(GGML_VULKAN ON CACHE BOOL "ggml: use Vulkan" FORCE)
        list(APPEND CMAKE_PREFIX_PATH "${_AI_CHAT_VULKAN_SDK}/Lib/cmake")
        set("SPIRV-Headers_DIR" "${_AI_CHAT_VULKAN_SDK}/Lib/cmake/SPIRV-Headers" CACHE PATH "SPIRV-Headers package directory" FORCE)
    else()
        message(WARNING "Vulkan SDK found at ${_AI_CHAT_VULKAN_SDK}, but no Windows host compiler is available for vulkan-shaders-gen. Vulkan backend disabled; use the Visual Studio developer prompt or build-android.bat.")
    endif()
else()
    message(STATUS "Vulkan SDK not available; falling back to other backends")
endif()

if (AI_CHAT_PREFER_OPENCL AND NOT DEFINED GGML_OPENCL)
    find_package(OpenCL QUIET)
    if (OpenCL_FOUND)
        message(STATUS "OpenCL toolchain detected; enabling ggml-opencl")
        set(GGML_OPENCL ON CACHE BOOL "ggml: use OpenCL" FORCE)
        set(GGML_OPENCL_EMBED_KERNELS ON CACHE BOOL "ggml: embed OpenCL kernels" FORCE)
        if (ANDROID_ABI STREQUAL "arm64-v8a")
            set(GGML_OPENCL_USE_ADRENO_KERNELS ON CACHE BOOL "ggml: use optimized kernels for Adreno" FORCE)
        else()
            set(GGML_OPENCL_USE_ADRENO_KERNELS OFF CACHE BOOL "ggml: use optimized kernels for Adreno" FORCE)
        endif()
    else()
        message(STATUS "OpenCL backend not available; using CPU fallback only")
    endif()
endif()

set(LLAMA_SRC ${CMAKE_CURRENT_LIST_DIR}/../../../../../../upstream/llama.cpp)
add_subdirectory(${LLAMA_SRC} build-llama)

add_library(${CMAKE_PROJECT_NAME} SHARED
        ai_chat.cpp)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        GGML_SYSTEM_ARCH=${GGML_SYSTEM_ARCH}
        GGML_CPU_KLEIDIAI=$<BOOL:${GGML_CPU_KLEIDIAI}>
        GGML_OPENMP=$<BOOL:${GGML_OPENMP}>
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${LLAMA_SRC}
        ${LLAMA_SRC}/common
        ${LLAMA_SRC}/include
        ${LLAMA_SRC}/ggml/include
        ${LLAMA_SRC}/ggml/src
        ${CMAKE_CURRENT_LIST_DIR}/../../../../../../cpp)

target_link_libraries(${CMAKE_PROJECT_NAME}
        llama
        llama-common
        android
        log)
