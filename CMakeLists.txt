cmake_minimum_required(VERSION 3.5)

project(graph LANGUAGES C)
set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic -ansi")

add_executable(server
    server.c
)