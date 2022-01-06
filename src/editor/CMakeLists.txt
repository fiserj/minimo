set(NAME editor)

add_executable(${NAME}
    editor.cpp
    editor_font.h
    editor_ted.h
    ted.h
)

target_include_directories(${NAME} PRIVATE
    .
    ${CMAKE_BINARY_DIR}/shaders/shaders
)

target_link_libraries(${NAME} PRIVATE
    ${PROJECT_NAME}
    bgfx
    tinycc
    utf8
)

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_compile_definitions(${NAME} PRIVATE
    TED_TESTS
    MNM_INCLUDE_PATH="${CMAKE_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_definitions(${NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )

    target_compile_options(${NAME} PRIVATE /Wall)
else()
    target_compile_options(${NAME} PRIVATE -Wall)
endif()

add_shader_dependency(${NAME} "shaders/text.vs")
add_shader_dependency(${NAME} "shaders/text.fs")