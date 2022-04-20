include(FetchContent)

FetchContent_Declare(
    enkiTS
    GIT_REPOSITORY https://github.com/dougbinks/enkiTS.git
    GIT_TAG        v1.11
)

FetchContent_Declare(
    gleq
    GIT_REPOSITORY https://github.com/glfw/gleq.git
    GIT_TAG        4dd5070341fa17856d06a38f948a100df2fc34cd
)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.7
)

FetchContent_MakeAvailable(
    enkiTS
    gleq
    glfw
)

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
    ${enkits_SOURCE_DIR}/src
)