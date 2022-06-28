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
    GIT_TAG        62e175ef9fae75335575964c845a302447c012c7
)

FetchContent_Declare(
    HandmadeMath
    GIT_REPOSITORY https://github.com/HandmadeMath/Handmade-Math.git
    GIT_TAG        v1.13.0
)

FetchContent_Declare(
    librope
    GIT_REPOSITORY https://github.com/fiserj/librope.git
    GIT_TAG        2bb786327f4abf95c38f32aa87378d2cb4dd85cf
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

FetchContent_Declare(
    tree-sitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG        v0.20.6
)

FetchContent_Declare(
    tree-sitter-c
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
    GIT_TAG        v0.20.1
)

FetchContent_Declare(
    utf8
    GIT_REPOSITORY https://github.com/sheredom/utf8.h.git
    GIT_TAG        ce48f0eda6cac8f365837edaf8298ad5c03f7f2e
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
FetchContent_Populate(librope)
FetchContent_Populate(meshoptimizer)
FetchContent_Populate(stb)
FetchContent_Populate(tinycc)
FetchContent_Populate(tree-sitter)
FetchContent_Populate(tree-sitter-c)

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

if(MNM_TESTS)
    TURN_OFF(CATCH_INSTALL_DOCS)
    TURN_OFF(CATCH_INSTALL_EXTRAS)
    TURN_OFF(CATCH_DEVELOPMENT_BUILD)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        f526ff0fc37ae00ff2c0dc8f6550bf8209c30afc
    )

    FetchContent_Declare(
        minimo_assets
        GIT_REPOSITORY https://github.com/fiserj/minimo.git
        GIT_TAG        1da992c73288f720974ddab3a18dedc940c62493
    )

    FetchContent_MakeAvailable(
        Catch2
        minimo_assets
    )
endif()

if(MNM_EDITOR)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        250333d895b1067533533dcfab137512745b9689
    )

    FetchContent_Declare(
        ImGuiColorTextEdit
        GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
        GIT_TAG        0a88824f7de8d0bd11d8419066caa7d3469395c4
    )

    FetchContent_Populate(imgui)
    FetchContent_Populate(ImGuiColorTextEdit)

    include(cmake/imgui.cmake)
    include(cmake/ImGuiColorTextEdit.cmake)
endif()