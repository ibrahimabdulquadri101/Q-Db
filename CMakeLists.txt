cmake_minimum_required(VERSION 3.10)
project(tiny-db)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES "src/*.cpp")

add_executable(tiny_db ${SOURCES})
