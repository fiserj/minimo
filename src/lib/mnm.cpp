#include <bgfx/bgfx.h>                 // bgfx::*

#include <bx/bx.h>                     // BX_COUNTOF
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

// Window creation flags.
enum
{
    WINDOW_FLAG_FULL_SCREEN  = 0x01,
    WINDOW_FLAG_FIXED_SIZE   = 0x02,
    WINDOW_FLAG_FIXED_ASPECT = 0x04,
};

// Creates window of specific size and attributes.
static GLFWwindow* create_window
(
    const char* title  = nullptr,
    int         width  = 0,
    int         height = 0,
    int         flags  = 0
)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor* monitor = nullptr;

    if (flags & WINDOW_FLAG_FULL_SCREEN)
    {
        monitor = glfwGetPrimaryMonitor();

        if (width <= 0 && height <= 0)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            width  = mode->width;
            height = mode->height;
        }
    }
    else if (flags & WINDOW_FLAG_FIXED_SIZE)
    {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    }

    width  = width  > 0 ? width  : 800;
    height = height > 0 ? height : 600;
    title  = title      ? title  : "Untitled";

    GLFWwindow* window = glfwCreateWindow(width, height, title, monitor, nullptr);

    if (window && (flags & WINDOW_FLAG_FIXED_ASPECT))
    {
        glfwSetWindowAspectRatio(window, width, height);
    }

    return window;
}

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

#include <mnm/mnm.h>
#include <mnm/window.h>

struct Context
{
    int   width;
    int   height;
    float dpi;
};

void mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    if (setup)
    {
        (*setup)();
    }

    // TODO : Run the draw function.

    if (cleanup)
    {
        (*cleanup)();
    }
}

