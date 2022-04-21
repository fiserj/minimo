cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME minimo)
project(${PROJECT_NAME})

add_subdirectory(third_party)
add_subdirectory(src)

option(MNM_TESTS "Build MiNiMo Tests" OFF)