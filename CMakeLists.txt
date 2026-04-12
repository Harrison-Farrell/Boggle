# Minimum CMake version required
cmake_minimum_required(VERSION  3.31.6)

# Project name and version
project(Solo-Boggle-Solver VERSION 1.0 LANGUAGES C CXX)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(LIBRARY_PREFIX "Solver")

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Check the Compiler Type
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wno-c2y-extensions)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g -march=native)
    endif()
else()
    message(FATAL_ERROR "This Project Requires Clang due supported C++23 features and built-in functions.")
endif()

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    include(UnitTest)
    add_subdirectory(test)
endif()

if(BUILD_BENCHMARKS)
    include(Benchmarking)
    add_subdirectory(benchmark)
endif()