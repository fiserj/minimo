set(NAME test)

add_executable(${NAME}
    hello_triangle.c
)

target_include_directories(${NAME} PRIVATE
    .
)

target_link_libraries(${NAME} PRIVATE
    ${PROJECT_NAME}_rwr
)

set_target_properties(${NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin$<$<CONFIG:Debug>:>"
)

if(MSVC)
    target_compile_options(${NAME} PRIVATE /Wall)
else()
    target_compile_options(${NAME} PRIVATE -Wall)
endif()