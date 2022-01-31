cmake_minimum_required(VERSION 3.16)
project(hallo)

set(CMAKE_CXX_STANDARD 14)

include_directories(${PROJECT_SOURCE_DIR}/def)

add_executable(hallo main.cpp def/util/buffer.h)
target_link_libraries(hallo uv)