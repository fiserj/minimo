cmake_policy(SET CMP0065 NEW)

function(add_project NAME IS_TEST_RUNNER)
    set(SOURCE_FILES "")

    if(APPLE)
        list(APPEND SOURCE_FILES
            ../mnm_platform.mm
        )
    else()
        list(APPEND SOURCE_FILES
            ../mnm_platform.cpp
        )
    endif()

    if(IS_TEST_RUNNER)
        list(APPEND SOURCE_FILES
            mnm_rwr_lib.cpp
        )

        add_executable(${NAME}
            ${SOURCE_FILES}
        )

        target_link_libraries(${NAME} PRIVATE
            Catch2::Catch2WithMain
        )
    else()
        list(APPEND SOURCE_FILES
            mnm_rwr_lib_tests.cpp
        )

        add_library(${NAME} STATIC
            ${SOURCE_FILES}
        )
    endif()

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
endfunction()

add_project(${PROJECT_NAME}_rwr OFF)

if(MNM_TESTS)
    add_project(${PROJECT_NAME}_rwr_tests ON)
endif()