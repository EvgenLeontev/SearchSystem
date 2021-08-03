cmake_minimum_required(VERSION 3.15)
project(SearchSystem)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(source SOURCE_LIST)

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

set (CMAKE_CXX_FLAGS "-Wall -Wpedantic")
