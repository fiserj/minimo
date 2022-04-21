add_library(HandmadeMath INTERFACE)

target_include_directories(HandmadeMath INTERFACE
    Handmade-Math
    ${handmademath_SOURCE_DIR}
)