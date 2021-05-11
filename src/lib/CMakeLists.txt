set(NAME ${PROJECT_NAME})

set(SOURCE_FILES
    mnm.cpp
    ../../include/mnm/mnm.h # Not needed for build, but IDE-friendly.
)

if(APPLE)
    list(APPEND SOURCE_FILES
        mnm_platform.mm
    )
else()
    list(APPEND SOURCE_FILES
        mnm_platform.cpp
    )
endif()

add_library(${NAME} STATIC
    ${SOURCE_FILES}
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
    enkiTS
    gleq
    glfw
    HandmadeMath
    meshoptimizer
)

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 17 # Needed for `__has_include`.
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

include(cmake/add_shader_dependency.cmake)

add_shader_dependency(${NAME} "shaders/poscolor.vs")
add_shader_dependency(${NAME} "shaders/poscolor.fs")