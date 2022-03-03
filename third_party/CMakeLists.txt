macro(TURN_OFF option)
    set(${option} OFF CACHE INTERNAL "" FORCE)
endmacro()

TURN_OFF(ENKITS_BUILD_C_INTERFACE)
TURN_OFF(ENKITS_BUILD_EXAMPLES)
TURN_OFF(ENKITS_BUILD_SHARED)
TURN_OFF(ENKITS_INSTALL)

TURN_OFF(GLFW_BUILD_DOCS)
TURN_OFF(GLFW_BUILD_EXAMPLES)
TURN_OFF(GLFW_BUILD_TESTS)
TURN_OFF(GLFW_DOCUMENT_INTERNALS)
TURN_OFF(GLFW_INSTALL)
TURN_OFF(GLFW_VULKAN_STATIC)

add_subdirectory(enkiTS)
add_subdirectory(glfw)

include(cmake/bgfx.cmake)
include(cmake/bimg.cmake)
include(cmake/bx.cmake)
include(cmake/gleq.cmake)
include(cmake/HandmadeMath.cmake)
include(cmake/librope.cmake)
include(cmake/meshoptimizer.cmake)
include(cmake/stb.cmake)
include(cmake/tinycc.cmake)
include(cmake/tree-sitter.cmake)
include(cmake/tree-sitter-c.cmake)
include(cmake/utf8.cmake)

target_include_directories(enkiTS PUBLIC
    enkiTS/src
)