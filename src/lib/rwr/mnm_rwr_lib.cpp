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
// PUBLIC API IMPLEMENTATION - INPUT
// -----------------------------------------------------------------------------

float mouse_x(void)
{
    return g_ctx->mouse.current.X;
}

float mouse_y(void)
{
    return g_ctx->mouse.current.Y;
}

float mouse_dx(void)
{
    return g_ctx->mouse.delta.X;
}

float mouse_dy(void)
{
    return g_ctx->mouse.delta.Y;
}

int mouse_down(int button)
{
    return g_ctx->mouse.is(button, InputState::DOWN);
}

int mouse_held(int button)
{
    return g_ctx->mouse.is(button, InputState::HELD);
}

int mouse_up(int button)
{
    return g_ctx->mouse.is(button, InputState::UP);
}

int mouse_clicked(int button)
{
    return g_ctx->mouse.repeated_click_count(button);
}

float mouse_held_time(int button)
{
    return g_ctx->mouse.held_time(button, f32(g_ctx->total_time.elapsed));
}

float scroll_x(void)
{
    return g_ctx->mouse.scroll.X;
}

float scroll_y(void)
{
    return g_ctx->mouse.scroll.Y;
}

int key_down(int key)
{
    return g_ctx->keyboard.is(key, InputState::DOWN);
}

int key_repeated(int key)
{
    return g_ctx->keyboard.is(key, InputState::REPEATED);
}

int key_held(int key)
{
    return g_ctx->keyboard.is(key, InputState::HELD);
}

int key_up(int key)
{
    return g_ctx->keyboard.is(key, InputState::UP);
}

float key_held_time(int key)
{
    return g_ctx->keyboard.held_time(key, f32(g_ctx->total_time.elapsed));
}

unsigned int codepoint(void)
{
    return int(next(g_ctx->codepoint_queue));
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TIME
// -----------------------------------------------------------------------------

double elapsed(void)
{
    return g_ctx->total_time.elapsed;
}

double dt(void)
{
    return g_ctx->frame_time.elapsed;
}

void sleep_for(double seconds)
{
    ASSERT(
        !t_ctx->is_main_thread,
        "`sleep_for` must not be called from the main thread."
    );

    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

void tic(void)
{
    tic(t_ctx->stop_watch);
}

double toc(void)
{
    return toc(t_ctx->stop_watch);
}


// -----------------------------------------------------------------------------
