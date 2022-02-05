set(TREE_SITTER_DIR tree-sitter/lib)

add_library(tree-sitter STATIC
    ${TREE_SITTER_DIR}/src/alloc.c
    ${TREE_SITTER_DIR}/src/get_changed_ranges.c
    ${TREE_SITTER_DIR}/src/language.c
    ${TREE_SITTER_DIR}/src/lexer.c
    ${TREE_SITTER_DIR}/src/lib.c
    ${TREE_SITTER_DIR}/src/node.c
    ${TREE_SITTER_DIR}/src/parser.c
    ${TREE_SITTER_DIR}/src/query.c
    ${TREE_SITTER_DIR}/src/stack.c
    ${TREE_SITTER_DIR}/src/subtree.c
    ${TREE_SITTER_DIR}/src/tree_cursor.c
    ${TREE_SITTER_DIR}/src/tree.c
)

target_include_directories(tree-sitter
    PUBLIC
        ${TREE_SITTER_DIR}/include
    PRIVATE
        ${TREE_SITTER_DIR}/src
)

set_target_properties(tree-sitter PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
)