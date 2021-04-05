set(BX_DIR bx)

add_library(bx STATIC
    ${BX_DIR}/src/amalgamated.cpp
)

target_include_directories(bx
    PUBLIC
        ${BX_DIR}/include
    PRIVATE
        ${BX_DIR}/3rdparty
)

if(APPLE)
    target_include_directories(bx PRIVATE
        ${BX_DIR}/include/compat/osx
    )
elseif(WIN32)
    target_include_directories(bx PUBLIC
        ${BX_DIR}/include/compat/msvc
    )
    target_compile_definitions(bx PRIVATE
        __STDC_FORMAT_MACROS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set_target_properties(bx PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)