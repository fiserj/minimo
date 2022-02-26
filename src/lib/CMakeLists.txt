cmake_policy(SET CMP0065 NEW)

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
    stb
)

if(MSVC)
    target_compile_definitions(${NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(${NAME} PRIVATE
        /Wall
    )
else()
    target_compile_options(${NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fdata-sections
        -ffunction-sections
        # -Wl,--gc-sections,--print-gc-sections
    )
    target_link_options(${NAME} PRIVATE
        --gc-sections
        --print-gc-sections
    )
endif()

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 17 # needed for `__has_include` (hard requirement)
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

add_shader_dependency(${NAME} "shaders/position.vs"                 )
add_shader_dependency(${NAME} "shaders/position.fs"                 )
add_shader_dependency(${NAME} "shaders/position_color.vs"           )
add_shader_dependency(${NAME} "shaders/position_color.fs"           )
add_shader_dependency(${NAME} "shaders/position_color_normal.vs"    )
add_shader_dependency(${NAME} "shaders/position_color_normal.fs"    )
add_shader_dependency(${NAME} "shaders/position_color_texcoord.vs"  )
add_shader_dependency(${NAME} "shaders/position_color_texcoord.fs"  )
add_shader_dependency(${NAME} "shaders/position_color_r_texcoord.fs")
add_shader_dependency(${NAME} "shaders/position_color_r_pixcoord.fs")
add_shader_dependency(${NAME} "shaders/position_normal.vs"          )
add_shader_dependency(${NAME} "shaders/position_normal.fs"          )
add_shader_dependency(${NAME} "shaders/position_texcoord.vs"        )
add_shader_dependency(${NAME} "shaders/position_texcoord.fs"        )

add_shader_dependency(${NAME} "shaders/instancing_position_color.vs")