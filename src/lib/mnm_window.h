#pragma once

namespace mnm
{

struct Window
{
    GLFWwindow* handle                = nullptr;
    i32         framebuffer_size[2]   = {};
    f32         invariant_size[2]     = {};
    f32         position_scale[2]     = {};
    f32         display_scale[2]      = {};
    f32         display_aspect        = 0.0f;
    bool        display_scale_changed = false;

    void update_size_info()
    {
        ASSERT(handle);

        i32 window_size[2];
        glfwGetWindowSize(handle, &window_size[0], &window_size[1]);

        glfwGetFramebufferSize(handle, &framebuffer_size[0], &framebuffer_size[1]);
        display_aspect = f32(framebuffer_size[0]) / framebuffer_size[1];

        const f32 prev_display_scale = display_scale[0];
        glfwGetWindowContentScale(handle, &display_scale[0], &display_scale[1]);

        display_scale_changed = prev_display_scale != display_scale[0];

        for (i32 i = 0; i < 2; i++)
        {
            if (display_scale[i] != 1.0 &&
                window_size[i] * display_scale[i] != f32(framebuffer_size[i]))
            {
                invariant_size[i] = framebuffer_size[i] / display_scale[i];
                position_scale[i] = 1.0f / display_scale[i];
            }
            else
            {
                invariant_size[i] = f32(window_size[i]);
                position_scale[i] = 1.0f;
            }
        }
    }
};

static void resize_window(GLFWwindow* window, i32 width, i32 height, i32 flags)
{
    ASSERT(window);
    ASSERT(flags >= 0);

    GLFWmonitor* monitor = glfwGetWindowMonitor(window);

    if (flags & WINDOW_FULL_SCREEN)
    {
        if (!monitor)
        {
            monitor = glfwGetPrimaryMonitor();
        }

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        if (width  <= 0) { width  = mode->width ; }
        if (height <= 0) { height = mode->height; }

        glfwSetWindowMonitor(window, monitor, 0, 0, width, height, GLFW_DONT_CARE);
    }
    else if (monitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        if (width  <= MIN_WINDOW_SIZE) { width  = DEFAULT_WINDOW_WIDTH ; }
        if (height <= MIN_WINDOW_SIZE) { height = DEFAULT_WINDOW_HEIGHT; }

        const i32 x = (mode->width  - width ) / 2;
        const i32 y = (mode->height - height) / 2;

        monitor = nullptr;

        glfwSetWindowMonitor(window, nullptr, x, y, width, height, GLFW_DONT_CARE);
    }

    // Other window aspects are ignored, if the window is currently in full screen mode.
    if (monitor)
    {
        return;
    }

    if (width  <= MIN_WINDOW_SIZE) { width  = DEFAULT_WINDOW_WIDTH ; }
    if (height <= MIN_WINDOW_SIZE) { height = DEFAULT_WINDOW_HEIGHT; }

    glfwSetWindowSize(window, width, height);

    if (flags & WINDOW_FIXED_ASPECT)
    {
        glfwSetWindowAspectRatio(window, width, height);
    }
    else
    {
        glfwSetWindowAspectRatio(window, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }

    const i32 resizable = (flags & WINDOW_FIXED_SIZE) ? GLFW_FALSE : GLFW_TRUE;
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, resizable);
}

} // namespace mnm
