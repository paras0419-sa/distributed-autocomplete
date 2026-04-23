cmake_minimum_required(VERSION 3.25)
project(distributed_autocomplete LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Warnings)
include(Sanitizers)

enable_testing()

add_subdirectory(libs/common)
# add_subdirectory(libs/memory)        # enabled in Task 1.1
# add_subdirectory(libs/index)         # enabled in Task 1.2
# add_subdirectory(libs/query)         # enabled in Task 1.5
add_subdirectory(bin/serving_node)
add_subdirectory(tests/unit)
