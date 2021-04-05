set(NAME ${PROJECT_NAME})

add_library(${NAME} SHARED
    window.cpp
)

target_include_directories(${NAME}
    PUBLIC
        ../../include
    PRIVATE
        .
)

target_link_libraries(${NAME} PRIVATE
    bgfx
    bx
    gleq
    glfw
)

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)