set(IGCTE_DIR ${imguicolortextedit_SOURCE_DIR})

add_library(ImGuiColorTextEdit STATIC
    ${IGCTE_DIR}/TextEditor.cpp
)

target_include_directories(ImGuiColorTextEdit PUBLIC
    ${IGCTE_DIR}
)

target_link_libraries(ImGuiColorTextEdit PUBLIC
    imgui
)

set_target_properties(ImGuiColorTextEdit PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)