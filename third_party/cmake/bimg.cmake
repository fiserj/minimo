set(BIMG_DIR ${bimg_SOURCE_DIR})

set(BIMG_SOURCE_FILES
    ${BIMG_DIR}/src/image.cpp
    ${BIMG_DIR}/src/image_gnf.cpp
)

# MSVC does not optimize-out the compile-time disabled (via BIMG_DECODE_ENABLE = 0)
# parts, so we have to provide fake implementation to remove the linker errors.
if(MSVC)
    list(APPEND BIMG_SOURCE_FILES "cmake/bimg_patch.cpp")
endif()

add_library(bimg STATIC
    ${BIMG_SOURCE_FILES}
)

target_compile_definitions(bimg PRIVATE
    -DBIMG_DECODE_ENABLE=0
)

target_include_directories(bimg
    PUBLIC
        ${BIMG_DIR}/include
    PRIVATE
        ${BIMG_DIR}/3rdparty/astc-codec/include
)

target_link_libraries(bimg PRIVATE
    bx
)

set_target_properties(bimg PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)