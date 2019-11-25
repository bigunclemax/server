cmake_minimum_required(VERSION 3.10)
project(server C)

set(CMAKE_C_STANDARD 99)

#include(engine/CMakeLists.txt)

add_executable(server main.c httpd.c httpd.h json-parser/json.c)
#add_executable(server server.c json-parser/json.c server.c)
get_filename_component(gost_lib ./engine/bin/gost.so ABSOLUTE)
message(${gost_lib})
target_link_libraries(server m /mnt/c/Users/bigun/CLionProjects/server/engine/bin/libgost.a)
