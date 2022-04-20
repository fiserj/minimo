set(BGFX_DIR ${bgfx_SOURCE_DIR})

if(APPLE)
    add_library(bgfx STATIC
        ${BGFX_DIR}/src/amalgamated.mm
    )
else()
    add_library(bgfx STATIC
        ${BGFX_DIR}/src/amalgamated.cpp
    )
endif()

target_include_directories(bgfx
    PUBLIC
        ${BGFX_DIR}/include
    PRIVATE
        ${BGFX_DIR}/3rdparty/khronos
)

if(APPLE)
    target_link_libraries(bgfx PUBLIC
        "-framework AppKit"
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
    )

    target_compile_definitions(bgfx PRIVATE
        BGFX_CONFIG_RENDERER_METAL
    )
elseif(WIN32)
    target_include_directories(bgfx PRIVATE
            ${BGFX_DIR}/3rdparty
            ${BGFX_DIR}/3rdparty/dxsdk/include
    )

    target_compile_definitions(bgfx PRIVATE
        BGFX_CONFIG_RENDERER_DIRECT3D11
    )
endif()

if (MSVC)
    target_compile_definitions(bgfx PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

target_link_libraries(bgfx
    PRIVATE
        bimg
    PUBLIC
        bx
)

set_target_properties(bgfx PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)