set(TREE_SITTER_C_DIR tree-sitter-c/src)

add_library(tree-sitter-c STATIC
    ${TREE_SITTER_C_DIR}/parser.c
)

target_include_directories(tree-sitter-c PRIVATE
    ${TREE_SITTER_C_DIR}
)

set_target_properties(tree-sitter-c PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
)