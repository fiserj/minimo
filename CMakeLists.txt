cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME minimo_assets)
project(${PROJECT_NAME})

# ------------------------------------------------------------------------------

add_library(minimo_assets_test_outputs INTERFACE)

target_include_directories(minimo_assets_test_outputs INTERFACE
    "test-outputs"
)

target_compile_definitions(minimo_assets_test_outputs INTERFACE
    MNM_ASSETS_TEST_OUTPUTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test-outputs"
)