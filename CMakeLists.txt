cmake_minimum_required(VERSION 3.10)
project(server C)

set(CMAKE_C_STANDARD 99)

add_library(gost engine/gosthash.c engine/gost89.c)
add_executable(server main.c httpd.c httpd.h)
target_link_libraries(server m crypto pthread gost)
target_include_directories(server PRIVATE jsmn engine)
