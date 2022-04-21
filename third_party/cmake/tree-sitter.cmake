set(TREE_SITTER_DIR "${tree-sitter_SOURCE_DIR}/lib")

add_library(tree-sitter STATIC
    ${TREE_SITTER_DIR}/src/lib.c
)

target_include_directories(tree-sitter
    PRIVATE
        ${TREE_SITTER_DIR}/src
    PUBLIC
        ${TREE_SITTER_DIR}/include
)

set_target_properties(tree-sitter PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
)