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

include(FetchContent)

FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.git
    GIT_TAG        3cc0de333528d546f1518ebb9111233eee168022
)

FetchContent_Declare(
    bimg
    GIT_REPOSITORY https://github.com/bkaradzic/bimg.git
    GIT_TAG        663f724186e26caf46494e389ed82409106205fb
)

FetchContent_Declare(
    bx
    GIT_REPOSITORY https://github.com/bkaradzic/bx.git
    GIT_TAG        ad018d47c6c107e2fe2f3ba0716f9e728ed59a39
)

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

FetchContent_Declare(
    HandmadeMath
    GIT_REPOSITORY https://github.com/HandmadeMath/Handmade-Math.git
    GIT_TAG        v1.13.0
)

FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        v0.17
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        af1a5bc352164740c1cc1354942b1c6b72eacb8a
)

FetchContent_Declare(
    tinycc
    GIT_REPOSITORY https://github.com/TinyCC/tinycc.git
    GIT_TAG        027b8fb9b88fe137447fb8bb1b61079be9702472
    SOURCE_DIR     "${FETCHCONTENT_BASE_DIR}/tinycc" # Needed for a fix, see `cmake/tinycc.cmake`.

)

FetchContent_MakeAvailable(
    enkiTS
    glfw
)

FetchContent_Populate(bgfx)
FetchContent_Populate(bimg)
FetchContent_Populate(bx)
FetchContent_Populate(gleq)
FetchContent_Populate(HandmadeMath)
FetchContent_Populate(meshoptimizer)
FetchContent_Populate(stb)
FetchContent_Populate(tinycc)

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