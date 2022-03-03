set(LIBROPE_DIR librope)

add_library(librope STATIC
    ${LIBROPE_DIR}/rope.c
    ${LIBROPE_DIR}/rope.h
)

target_include_directories(librope PUBLIC
    ${LIBROPE_DIR}
)

set_target_properties(librope PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
)