set(NAME test)

add_executable(${NAME}
    font_atlas.c
)

target_include_directories(${NAME} PRIVATE
    .
)

target_link_libraries(${NAME} PRIVATE
    ${PROJECT_NAME}
)

set_target_properties(${NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

if(MSVC)
    target_compile_options(${NAME} PRIVATE /Wall)
else()
    target_compile_options(${NAME} PRIVATE -Wall)
endif()