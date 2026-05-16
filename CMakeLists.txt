cmake_minimum_required(VERSION 3.14)

project(Cpp_SIMT_GPU_Emulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simt_gpu_core
    src/cache.cpp
    src/config.cpp
    src/gpu.cpp
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
)

target_include_directories(simt_gpu_core PUBLIC include)

add_executable(simt_gpu src/main.cpp)
target_link_libraries(simt_gpu PRIVATE simt_gpu_core)

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
)

foreach(test_name IN LISTS SIMT_TESTS)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE simt_gpu_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
