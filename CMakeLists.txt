cmake_minimum_required(VERSION 3.14)

project(Cpp_SIMT_GPU_Emulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(ENABLE_ASAN "Enable AddressSanitizer on supported compilers" OFF)
option(ENABLE_TRACE "Enable trace-oriented compile definitions" OFF)
option(ENABLE_TESTS "Build and register tests" ON)

add_library(simt_gpu_core
    src/cache.cpp
    src/config.cpp
    src/gpu.cpp
    src/invariant_checker.cpp
    src/sm.cpp
    src/warp.cpp
    src/lane.cpp
    src/instruction.cpp
    src/isa.cpp
    src/parser.cpp
    src/memory.cpp
    src/shared_memory.cpp
    src/global_memory.cpp
    src/memory_coalescer.cpp
    src/memory_request.cpp
    src/occupancy.cpp
    src/warp_scheduler.cpp
    src/scoreboard.cpp
    src/divergence_stack.cpp
    src/functional_unit.cpp
    src/stats.cpp
    src/trace.cpp
    src/reference_gpu.cpp
)

target_include_directories(simt_gpu_core PUBLIC include)

if(ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(simt_gpu_core PRIVATE /W4 /permissive-)
    else()
        target_compile_options(simt_gpu_core PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(ENABLE_ASAN AND NOT MSVC)
    target_compile_options(simt_gpu_core PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(simt_gpu_core PUBLIC -fsanitize=address)
endif()

if(ENABLE_TRACE)
    target_compile_definitions(simt_gpu_core PUBLIC SIMT_GPU_ENABLE_TRACE=1)
endif()

add_executable(simt_gpu src/main.cpp)
target_link_libraries(simt_gpu PRIVATE simt_gpu_core)

if(ENABLE_TESTS)
    enable_testing()

    set(SIMT_TESTS
        test_alu
        test_warp_execution
        test_scoreboard
        test_memory_latency_hiding
        test_global_load_store
        test_shared_memory
        test_memory_coalescing
        test_branch_divergence
        test_scheduler
        test_config_loading
        test_l1_cache
        test_improved_coalescing
        test_warp_primitives
        test_barrier_sync
        test_occupancy
        test_kernel_launch
        test_timeline_output
        test_shared_memory_conflicts
        test_reference_gpu
        test_gpu_differential
        test_gpu_invariants
        test_random_gpu_programs
        test_divergence_reconvergence_correctness
        test_barrier_correctness
        test_memory_coalescer_correctness
        test_shared_memory_bank_conflicts_correctness
    )

    foreach(test_name IN LISTS SIMT_TESTS)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE simt_gpu_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
