cmake_policy(SET CMP0065 NEW)

set(NAME ${PROJECT_NAME}_rwr)

set(SOURCE_FILES
    mnm_rwr_test.cpp
)

if(APPLE)
    list(APPEND SOURCE_FILES
        ../mnm_platform.mm
    )
else()
    list(APPEND SOURCE_FILES
        ../mnm_platform.cpp
    )
endif()

add_executable(${NAME}
    ${SOURCE_FILES}
)

target_include_directories(${NAME} PUBLIC
    ../../../include

    # Fix for not building the same shaders twice.
    ..
    ${CMAKE_BINARY_DIR}/shaders
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

        -Wno-unused-const-variable
        -Wno-unused-function
    )
    # target_link_options(${NAME} PRIVATE
    #     --gc-sections
    #     --print-gc-sections
    # )
endif()

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 17 # needed for `__has_include` (hard requirement)
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)