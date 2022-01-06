set(TCC_DIR tinycc)

# Patch the variable-length array usage for MSVC.
if(MSVC)
    set(TINYCC_PATCH_DIR "${PROJECT_BINARY_DIR}/tinycc-patch")

    if(NOT EXISTS ${TINYCC_PATCH_DIR})
        file(COPY ${TCC_DIR} DESTINATION ${TINYCC_PATCH_DIR})

        set(X86_64_GEN_FILE_PATH   "${TINYCC_PATCH_DIR}/tinycc/x86_64-gen.c")
        set(X86_64_GEN_OLD_CONTENT "char _onstack[nb_args ? nb_args : 1], *onstack = _onstack;")
        set(X86_64_GEN_NEW_CONTENT "char _onstack[8], *onstack = _onstack;\n    assert(sizeof(_onstack) >= nb_args);")

        file(READ ${X86_64_GEN_FILE_PATH} X86_64_GEN_FILE_CONTENT)
        string(REPLACE "${X86_64_GEN_OLD_CONTENT}" "${X86_64_GEN_NEW_CONTENT}" X86_64_GEN_FILE_CONTENT "${X86_64_GEN_FILE_CONTENT}")

        file(WRITE ${X86_64_GEN_FILE_PATH} "${X86_64_GEN_FILE_CONTENT}")
    endif()

    set(TCC_DIR "${TINYCC_PATCH_DIR}/tinycc")
endif()

add_library(tinycc STATIC
    ${TCC_DIR}/libtcc.c
)

target_include_directories(tinycc
    PUBLIC
        ${TCC_DIR}
    PRIVATE
        ${PROJECT_BINARY_DIR}/tinycc-config
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tinycc.config.h.in"
    "${PROJECT_BINARY_DIR}/tinycc-config/config.h"
)