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

target_compile_definitions(bx PUBLIC
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
)

target_compile_definitions(bx PRIVATE
    "$<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>"
)

if(BX_CONFIG_DEBUG)
    target_compile_definitions(bx PRIVATE
        BX_CONFIG_DEBUG=1
    )
endif()

if(APPLE)
    target_include_directories(bx PRIVATE
        ${BX_DIR}/include/compat/osx
    )
elseif(WIN32)
    target_include_directories(bx PUBLIC
        ${BX_DIR}/include/compat/msvc
    )
endif()

if(MSVC)
    target_compile_definitions(bx PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set_target_properties(bx PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)