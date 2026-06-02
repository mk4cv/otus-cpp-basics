cmake_minimum_required(VERSION 3.7.2)
project(topk_words CXX)
project(topk_words_parallel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(topk_words
        topk_words.cpp
        )

add_executable(topk_words_parallel
        topk_words_parallel.cpp
        )

find_package(Threads REQUIRED)
target_link_libraries(topk_words PRIVATE Threads::Threads)
target_link_libraries(topk_words_parallel PRIVATE Threads::Threads)