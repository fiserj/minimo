add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_glfw.h
)

target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)

target_link_libraries(imgui PUBLIC
    glfw
)

set_target_properties(imgui PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)