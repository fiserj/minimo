cmake_policy(SET CMP0065 NEW)

function(add_project NAME PROJECT_TYPE)
    set(VALID_PROJECT_TYPES
        "editor"
        "library"
        "tests"
    )

    if(NOT PROJECT_TYPE IN_LIST VALID_PROJECT_TYPES)
        message(
            FATAL_ERROR
            "Invalid project type '${PROJECT_TYPE}'. Must be one of: '${VALID_PROJECT_TYPES}'."
        )
    endif()

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

    if(PROJECT_TYPE STREQUAL "library")
        list(APPEND SOURCE_FILES
            mnm_rwr_lib.cpp
        )

        add_library(${NAME} STATIC
            ${SOURCE_FILES}
        )
    elseif(PROJECT_TYPE STREQUAL "editor")
        list(APPEND SOURCE_FILES
            mnm_rwr_editor.cpp
        )

        add_executable(${NAME}
            ${SOURCE_FILES}
        )

        target_link_libraries(${NAME} PRIVATE
            imgui
            ImGuiColorTextEdit
        )
    elseif(PROJECT_TYPE STREQUAL "tests")
        list(APPEND SOURCE_FILES
            mnm_rwr_lib_tests.cpp
        )

        add_executable(${NAME}
            ${SOURCE_FILES}
        )

        target_link_libraries(${NAME} PRIVATE
            Catch2::Catch2WithMain
            minimo_assets_test_outputs
        )

        target_include_directories(${NAME} PRIVATE
            ../../test
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
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin$<$<CONFIG:Debug>:>" # The dummy generator expression
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin$<$<CONFIG:Debug>:>" # is to avoid having to list all
    )                                                                         # *_OUTPUT_DIRECTORY_* properties.
endfunction()

add_project(${PROJECT_NAME}_rwr "library")

if(MNM_EDITOR)
    add_project(${PROJECT_NAME}_rwr_editor "editor")
endif()

if(MNM_TESTS)
    add_project(${PROJECT_NAME}_rwr_tests "tests")
endif()