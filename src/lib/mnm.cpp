#include <assert.h>                    // assert
#include <stdint.h>                    // uint32_t
#include <string.h>                    // memcpy

#include <vector>                      // vector

#include <bgfx/bgfx.h>                 // bgfx::*
#include <bgfx/embedded_shader.h>      // BGFX_EMBEDDED_SHADER*

#include <bx/bx.h>                     // BX_COUNTOF
#include <bx/endian.h>                 // endianSwap
#include <bx/platform.h>               // BX_PLATFORM_*

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>                // glfw*

#if BX_PLATFORM_LINUX
#   define GLFW_EXPOSE_NATIVE_X11
#   define GLFW_EXPOSE_NATIVE_GLX
#elif BX_PLATFORM_OSX
#   define GLFW_EXPOSE_NATIVE_COCOA
#   define GLFW_EXPOSE_NATIVE_NSGL
#elif BX_PLATFORM_WINDOWS
#   define GLFW_EXPOSE_NATIVE_WIN32
#   define GLFW_EXPOSE_NATIVE_WGL
#endif
#include <GLFW/glfw3native.h>          // glfwGetX11Display, glfwGet*Window

#if BX_PLATFORM_OSX
#   import <Cocoa/Cocoa.h>             // NSWindow
#   import <QuartzCore/CAMetalLayer.h> // CAMetalLayer
#endif

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

#include <shaders/poscolor_fs.h>       // poscolor_fs
#include <shaders/poscolor_vs.h>       // poscolor_vs

#include <mnm/mnm.h>
#include <mnm/geometry.h>
#include <mnm/window.h>

// Creates BGFX-specific platform data.
static bgfx::PlatformData create_platform_data
(
    GLFWwindow*              window,
    bgfx::RendererType::Enum renderer
)
{
    assert(window);

    bgfx::PlatformData data;
#if BX_PLATFORM_LINUX
    data.ndt = glfwGetX11Display();
    data.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
#elif BX_PLATFORM_OSX
    data.nwh = glfwGetCocoaWindow(window);
#elif BX_PLATFORM_WINDOWS
    data.nwh = glfwGetWin32Window(window);
#endif

#if BX_PLATFORM_OSX
    // Momentary fix for https://github.com/bkaradzic/bgfx/issues/2036.
    if (renderer == bgfx::RendererType::Metal ||
        renderer == bgfx::RendererType::Count)
    {
        bgfx::RendererType::Enum types[bgfx::RendererType::Count];
        const int n = bgfx::getSupportedRenderers(BX_COUNTOF(types), types);

        for (int i = 0; i < n; i++)
        {
            if (types[i] == bgfx::RendererType::Metal)
            {
                CAMetalLayer* layer = [CAMetalLayer layer];

                NSWindow* ns_window = static_cast<NSWindow*>(data.nwh);
                ns_window.contentView.layer = layer;
                ns_window.contentView.wantsLayer = YES;

                data.nwh = layer;

                break;
            }
        }
    }
#endif

    return data;
}

struct Vertex
{
    hmm_vec3 position;
    uint32_t color;
};

template <typename T>
struct Stack
{
    T              top;
    std::vector<T> data;

    void push()
    {
        data.push_back(top);
    }

    void pop()
    {
        top = data.back();
        data.pop_back();
    }
};

struct Context
{
    Vertex              state;
    std::vector<Vertex> vertices;
    Stack<hmm_mat4>     matrices = { HMM_Mat4d(1.0f) };
    GLFWwindow*         window   = nullptr;
};

static Context& get_context()
{
    static thread_local Context s_ctx;
    return s_ctx;
}

void size(int width, int height, int flags)
{
    assert(flags >= 0);

    GLFWwindow* window = get_context().window;
    assert(window);

    constexpr int MIN_SIZE      = 240;
    constexpr int DEFAULT_WIDTH = 640;
    constexpr int DEFAULT_HEIGHT= 480;

    // Current monitor.
    GLFWmonitor* monitor = glfwGetWindowMonitor(window);

    // Activate full screen mode, or adjust its resolution.
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

    // If in full screen mode, jump out of it.
    else if (monitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        if (width  <= MIN_SIZE) { width  = DEFAULT_WIDTH ; }
        if (height <= MIN_SIZE) { height = DEFAULT_HEIGHT; }

        const int x = (mode->width  - width ) / 2;
        const int y = (mode->height - height) / 2;

        monitor = nullptr;

        glfwSetWindowMonitor(window, nullptr, x, y, width, height, GLFW_DONT_CARE);
    }

    // Other window aspects are ignored, if the window is currently in full screen mode.
    if (monitor)
    {
        return;
    }

    // Size.
    if (width  <= MIN_SIZE) { width  = DEFAULT_WIDTH ; }
    if (height <= MIN_SIZE) { height = DEFAULT_HEIGHT; }

    glfwSetWindowSize(window, width, height);

    // Fixed aspect ratio.
    if (flags & WINDOW_FIXED_ASPECT)
    {
        glfwSetWindowAspectRatio(window, width, height);
    }
    else
    {
        glfwSetWindowAspectRatio(window, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }

    // Resize-ability (we might want to first check the current value).
    const int resizable = (flags & WINDOW_FIXED_SIZE) ? GLFW_FALSE : GLFW_TRUE;
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, resizable);
}

void title(const char* title)
{
    GLFWwindow* window = get_context().window;
    assert(window);

    glfwSetWindowTitle(window, title);
}

static void submit_immediate_geometry
(
    bgfx::ViewId               view,
    bgfx::ProgramHandle        program,
    const bgfx::VertexLayout&  layout,
    const std::vector<Vertex>& vertices
)
{
    const uint32_t num_vertices = static_cast<uint32_t>(vertices.size());

    if (num_vertices > bgfx::getAvailTransientVertexBuffer(num_vertices, layout))
    {
        assert(false);
        return;
    }

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, num_vertices, layout);
    memcpy(tvb.data, vertices.data(), num_vertices * sizeof(Vertex));

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setState(BGFX_STATE_DEFAULT);
    bgfx::submit(view, program);
}

int mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    if (glfwInit() != GLFW_TRUE)
    {
        return 1;
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    Context& ctx = get_context();

    ctx.window = glfwCreateWindow(640, 480, "MiNiMo", nullptr, nullptr);
    if (!ctx.window)
    {
        glfwTerminate();
        return 2;
    }

    bgfx::Init init;
    init.platformData = create_platform_data(ctx.window, init.type);

    if (!bgfx::init(init))
    {
        glfwDestroyWindow(ctx.window);
        glfwTerminate();
        return 3;
    }

    if (setup)
    {
        (*setup)();
    }

    bgfx::setDebug(BGFX_DEBUG_STATS);

    int last_fb_width  = 0;
    int last_fb_height = 0;

    constexpr bgfx::ViewId DEFAULT_VIEW = 0;

    const bgfx::RendererType::Enum    type        = bgfx::getRendererType();
    static const bgfx::EmbeddedShader s_shaders[] =
    {
        BGFX_EMBEDDED_SHADER(poscolor_fs),
        BGFX_EMBEDDED_SHADER(poscolor_vs),

        BGFX_EMBEDDED_SHADER_END()
    };

    bgfx::ProgramHandle program = bgfx::createProgram
    (
        bgfx::createEmbeddedShader(s_shaders, type, "poscolor_vs"),
        bgfx::createEmbeddedShader(s_shaders, type, "poscolor_fs"),
        true
    );
    assert(bgfx::isValid(program));

    bgfx::VertexLayout layout;
    layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0  , 4, bgfx::AttribType::Uint8, true)
        .end();

    while (!glfwWindowShouldClose(ctx.window) && glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) != GLFW_PRESS)
    {
        glfwPollEvents();

        int curr_fb_width  = 0;
        int curr_fb_height = 0;
        glfwGetFramebufferSize(ctx.window, &curr_fb_width, &curr_fb_height);

        if (curr_fb_width  != last_fb_width ||
            curr_fb_height != last_fb_height)
        {
            const uint16_t width  = static_cast<uint16_t>(curr_fb_width );
            const uint16_t height = static_cast<uint16_t>(curr_fb_height);

            bgfx::reset(width, height, BGFX_RESET_VSYNC);

            bgfx::setViewClear(DEFAULT_VIEW, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x333333ff);
            bgfx::setViewRect (DEFAULT_VIEW, 0, 0, width, height);

            last_fb_width  = curr_fb_width;
            last_fb_height = curr_fb_height;
        }

        bgfx::touch(DEFAULT_VIEW);

        // TODO : This needs to be done for all contexts across all threads.
        get_context().vertices.clear();

        if (draw)
        {
            (*draw)();
        }

        // TODO : This needs to be done for all contexts across all threads.
        submit_immediate_geometry(DEFAULT_VIEW, program, layout, get_context().vertices);

        bgfx::frame();
    }

    if (cleanup)
    {
        (*cleanup)();
    }

    bgfx::shutdown();

    glfwDestroyWindow(ctx.window);
    glfwTerminate();

    return 0;
}

void begin(void)
{
    assert(get_context().vertices.size() % 3 == 0);
}

void end()
{
    begin();
}

void vertex(float x, float y, float z)
{
    Context& ctx = get_context();

    ctx.state.position = (ctx.matrices.top * HMM_Vec4(x, y, z, 1.0f)).XYZ;
    ctx.vertices.push_back(ctx.state);
}

void color(unsigned int rgba)
{
    get_context().state.color = bx::endianSwap(rgba);
}