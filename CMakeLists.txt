cmake_minimum_required(VERSION 3.25)
project(virtual_machines)

set(CMAKE_CXX_FLAGS_ASAN            "-g -fsanitize=address,undefined -fno-sanitize-recover=all -D_GLIBCXX_DEBUG"
        CACHE STRING "Compiler flags in asan build"
        FORCE)
set(CMAKE_CXX_FLAGS "-DDEBUG ${CMAKE_CXX_FLAGS_ASAN} -Wall -Wextra -Wpedantic -Werror -Wsign-compare")

set(CMAKE_EXPORT_COMPILE_COMMANDS   ON)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(memory_pool)
add_subdirectory(string_refcount)