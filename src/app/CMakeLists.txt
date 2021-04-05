set(NAME ${PROJECT_NAME}_app)

add_executable(${NAME}
    app.cpp
)

target_include_directories(${NAME} PRIVATE
    .
)

target_link_libraries(${NAME} PRIVATE
    ${PROJECT_NAME}
)

set_target_properties(${NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)