cmake_minimum_required(VERSION 3.28)

project(common LANGUAGES CXX)

# Keep the target distinct from the legacy C helper in the root build.
add_library(common_cpp STATIC src/status.cpp)
add_library(common::common ALIAS common_cpp)

target_include_directories(common_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(common_cpp PUBLIC cxx_std_20)
set_target_properties(common_cpp PROPERTIES CXX_EXTENSIONS OFF)
