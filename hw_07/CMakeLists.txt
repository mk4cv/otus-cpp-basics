cmake_minimum_required(VERSION 3.5)

project(07.homework)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)

FetchContent_MakeAvailable(googletest)

enable_testing()

get_filename_component(CONTAINERS_PATH "${CMAKE_SOURCE_DIR}/../hw_06" ABSOLUTE)

add_executable(
    containers_tests
    sequential_container_tests.cpp
    dl_list_tests.cpp
    # ${CONTAINERS_PATH}/containers.cpp // Отдельная компиляция containers.cpp, вероятно, не требуется для тестов
    
)

target_compile_definitions(containers_tests PUBLIC BUILD_TESTS)

target_include_directories(containers_tests PUBLIC "${CONTAINERS_PATH}")

target_link_libraries(containers_tests PRIVATE GTest::gtest_main)

target_compile_options(containers_tests PRIVATE -Wno-unused-parameter)

add_test(NAME containers_tests_run COMMAND containers_tests)

add_custom_target(tests
    COMMAND $<TARGET_FILE:containers_tests>  
    DEPENDS containers_tests                
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit tests for containers..."
    VERBATIM
)

set_target_properties(containers_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)