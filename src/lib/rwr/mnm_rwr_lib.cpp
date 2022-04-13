#include "mnm_rwr.cpp"

using namespace mnm::rwr;

// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - WINDOW
// -----------------------------------------------------------------------------

void size(int width, int height, int flags)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`size` must be called from main thread only."
    );

    ASSERT(
        g_ctx->window_info.display_scale.X > 0.0f,
        "Invalid horizontal display scale (%.1f).",
        g_ctx->window_info.display_scale.X
    );

    ASSERT(
        g_ctx->window_info.display_scale.Y > 0.0f,
        "Invalid vertical display scale (%.1f).",
        g_ctx->window_info.display_scale.Y
    );

    // TODO : Round instead?
    if (g_ctx->window_info.position_scale.X != 1.0f)
    {
        width = int(width * g_ctx->window_info.display_scale.X);
    }

    // TODO : Round instead?
    if (g_ctx->window_info.position_scale.Y != 1.0f)
    {
        height = int(height * g_ctx->window_info.display_scale.Y);
    }

    resize_window(g_ctx->window_handle, width, height, flags);
}

void title(const char* title)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`title` must be called from main thread only."
    );

    glfwSetWindowTitle(g_ctx->window_handle, title);
}

void vsync(int vsync)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`vsync` must be called from main thread only."
    );

    g_ctx->vsync_on          = bool(vsync);
    g_ctx->reset_back_buffer = true;
}

void quit(void)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`quit` must be called from main thread only."
    );

    glfwSetWindowShouldClose(g_ctx->window_handle, GLFW_TRUE);
}

float width(void)
{
    return g_ctx->window_info.invariant_size.X;
}

float height(void)
{
    return g_ctx->window_info.invariant_size.Y;
}

float aspect(void)
{
    return
        f32(g_ctx->window_info.framebuffer_size.X) /
        f32(g_ctx->window_info.framebuffer_size.Y);
}

float dpi(void)
{
    return g_ctx->window_info.display_scale.X;
}

int dpi_changed(void)
{
    return g_ctx->window_info.display_scale_changed || !g_ctx->frame_number;
}

int pixel_width(void)
{
    return g_ctx->window_info.framebuffer_size.X;
}

int pixel_height(void)
{
    return g_ctx->window_info.framebuffer_size.Y;
}


// -----------------------------------------------------------------------------
/// PUBLIC API IMPLEMENTATION - CURSOR
// -----------------------------------------------------------------------------

void cursor(int type)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`cursor` must be called from main thread only."
    );

    ASSERT(
        type >= CURSOR_ARROW && type <= CURSOR_LOCKED,
        "Invalid cursor type %i.",
        type
    );

    if (g_ctx->active_cursor != u32(type))
    {
        g_ctx->active_cursor = u32(type);

        switch (type)
        {
        case CURSOR_HIDDEN:
            glfwSetInputMode(g_ctx->window_handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case CURSOR_LOCKED:
            glfwSetInputMode(g_ctx->window_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        default:
            if (type >= CURSOR_ARROW && type <= CURSOR_LOCKED)
            {
                glfwSetInputMode(g_ctx->window_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursor   (g_ctx->window_handle, g_ctx->window_cursors[u32(type)]);
            }
        }
    }
}


// -----------------------------------------------------------------------------
