set(NAME test)

add_executable(${NAME}
    # test.cpp
    transient_geometry.cpp
)

target_include_directories(${NAME} PRIVATE
    .
)

target_link_libraries(${NAME} PRIVATE
    ${PROJECT_NAME}
)

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

if(MSVC)
    target_compile_options(${NAME} PRIVATE /Wall)
else()
    target_compile_options(${NAME} PRIVATE -Wall)
endif()