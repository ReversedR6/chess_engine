cmake_minimum_required(VERSION 3.16)
project(blitz CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wshadow -Wconversion)

add_executable(blitz
src/uci.cpp
src/board.cpp
src/movegen.cpp
src/eval.cpp
src/search.cpp
)

target_include_directories(blitz PRIVATE src)