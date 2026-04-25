cmake_minimum_required(VERSION 3.5)

project(07.homework)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)

FetchContent_MakeAvailable(googletest)

add_executable(
    containers_tests
    sequential_container_tests.cpp
    dl_list_tests.cpp
    containers.hpp
)
target_link_libraries(containers_tests PRIVATE GTest::gtest_main)

target_compile_options(containers_tests PRIVATE -Wno-unused-parameter)

set_target_properties(containers_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)