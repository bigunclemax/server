cmake_minimum_required(VERSION 3.10)

set(CMAKE_VERBOSE_MAKEFILE ON)

project(server C)

set(CMAKE_C_STANDARD 99)

add_subdirectory(engine)
add_executable(server main.c httpd.c)
target_link_libraries(server m crypto pthread gost)
target_include_directories(server PRIVATE jsmn)
