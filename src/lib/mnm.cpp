#include <assert.h>                    // assert
#include <stdint.h>                    // uint32_t
#include <string.h>                    // memcpy

#include <chrono>                      // duration
#include <mutex>                       // lock_guard, mutex
#include <thread>                      // this_thread
#include <vector>                      // vector

#include <bgfx/bgfx.h>                 // bgfx::*
#include <bgfx/embedded_shader.h>      // BGFX_EMBEDDED_SHADER*

#include <bx/bx.h>                     // BX_COUNTOF
#include <bx/endian.h>                 // endianSwap
#include <bx/platform.h>               // BX_PLATFORM_*
#include <bx/timer.h>                  // getHPCounter, getHPFrequency

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

#define GLEQ_IMPLEMENTATION
#define GLEQ_STATIC
#include <gleq.h>

#if BX_PLATFORM_OSX
#   import <Cocoa/Cocoa.h>             // NSWindow
#   import <QuartzCore/CAMetalLayer.h> // CAMetalLayer
#endif

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

#include <TaskScheduler.h>             // ... 

#include <shaders/poscolor_fs.h>       // poscolor_fs
#include <shaders/poscolor_vs.h>       // poscolor_vs

#include <mnm/mnm.h>

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

struct Attribs
{
    uint32_t color;
};

struct Vertex
{
    hmm_vec3 position;
    Attribs  attribs;
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

struct MatrixStack : Stack<hmm_mat4>
{
    void mul(const hmm_mat4& matrix)
    {
        top = matrix * top;
    }
};

template <int MAX_INPUTS, typename T>
struct InputState
{
    enum : uint8_t
    {
        DOWN = 0x01,
        UP   = 0x02,
        HELD = 0x04,
    };

    static constexpr int INVALID_INPUT =  -1;

    uint8_t states[MAX_INPUTS] = { 0 };

    inline bool is(int app_input, int flag) const
    {
        const int input = T::translate_app_input(app_input);

        return (input > INVALID_INPUT && input < MAX_INPUTS)
            ? states[input] & flag
            : false;
    }

    void update_input_state(int input, bool down)
    {
        if (input > INVALID_INPUT && input < MAX_INPUTS)
        {
            states[input] |= down ? DOWN : UP;
        }
    }

    void update_state_flags()
    {
        for (int i = 0; i < MAX_INPUTS; i++)
        {
            if (states[i] & UP)
            {
                states[i] = 0;
            }
            else if (states[i] & DOWN)
            {
                states[i] = HELD;
            }
        }
    }
};

struct Mouse : InputState<GLFW_MOUSE_BUTTON_LAST, Mouse>
{
    int curr [2] = { 0 };
    int prev [2] = { 0 };
    int delta[2] = { 0 };

    void update_position(int x, int y)
    {
        curr[0] = x;
        curr[1] = y;
    }

    void update_position_delta()
    {
        delta[0] = curr[0] - prev[0];
        delta[1] = curr[1] - prev[1];

        prev[0] = curr[0];
        prev[1] = curr[1];
    }

    static int translate_app_input(int app_button)
    {
        switch (app_button)
        {
        case MOUSE_LEFT:
            return GLFW_MOUSE_BUTTON_LEFT;
        case MOUSE_RIGHT:
            return GLFW_MOUSE_BUTTON_RIGHT;
        case MOUSE_MIDDLE:
            return GLFW_MOUSE_BUTTON_MIDDLE;
        default:
            return INVALID_INPUT;
        }
    }
};

struct Keyboard : InputState<GLFW_KEY_LAST, Keyboard>
{
    static int translate_app_input(int app_key)
    {
        static const int special_app_keys[] =
        {
            0,                  // KEY_ANY
            GLFW_KEY_BACKSPACE, // KEY_BACKSPACE
            GLFW_KEY_DELETE,    // KEY_DELETE
            GLFW_KEY_DOWN,      // KEY_DOWN
            GLFW_KEY_ENTER,     // KEY_ENTER
            GLFW_KEY_ESCAPE,    // KEY_ESCAPE
            GLFW_KEY_LEFT,      // KEY_LEFT
            GLFW_KEY_RIGHT,     // KEY_RIGHT
            GLFW_KEY_SPACE,     // KEY_SPACE
            GLFW_KEY_TAB,       // KEY_TAB
            GLFW_KEY_UP,        // KEY_UP
        };

        int glfw_key = INVALID_INPUT;

        if (app_key >= 0 && app_key < BX_COUNTOF(special_app_keys))
        {
            glfw_key = special_app_keys[app_key];
        }
        else if (app_key >= 'A' && app_key <= 'Z')
        {
            glfw_key = app_key + (GLFW_KEY_A - 'A');
        }
        else if (app_key >= 'a' && app_key <= 'z')
        {
            glfw_key = app_key + (GLFW_KEY_A - 'a');
        }

        return glfw_key;
    }
};

struct Timer 
{
    static const double frequency;

    int64_t counter = 0;
    double  elapsed = 0.0;

    void tic()
    {
        counter = bx::getHPCounter();
    }

    double toc(bool restart = false)
    {
        const int64_t now = bx::getHPCounter();

        elapsed = (now - counter) / frequency;

        if (restart)
        {
            counter = now;
        }

        return elapsed;
    }
};

const double Timer::frequency = static_cast<double>(bx::getHPFrequency());

struct GeometryBuilder;

struct VertexVariantInfo
{
    enum
    {
        POSITION,
        COLOR,
        NORMAL,
        TEXCOORD,
    };

    typedef void (*AttribPushFunc)(GeometryBuilder&);

    static constexpr int MAX_VERTEX_ATTRS = 4;
    static constexpr int MAX_VERTEX_TYPES = 1 + (VERTEX_COLOR | VERTEX_TEXCOORD | VERTEX_NORMAL);

    AttribPushFunc     funcs  [MAX_VERTEX_TYPES] = { nullptr };
    bgfx::VertexLayout layouts[MAX_VERTEX_ATTRS];

    VertexVariantInfo()
    {
        #define ADD_VERTEX_TYPE(flags) funcs[flags] = attrib_push_func<flags>

        ADD_VERTEX_TYPE(0                                              );
        ADD_VERTEX_TYPE(VERTEX_COLOR                                   );
        ADD_VERTEX_TYPE(VERTEX_NORMAL                                  );
        ADD_VERTEX_TYPE(VERTEX_TEXCOORD                                );
        ADD_VERTEX_TYPE(VERTEX_COLOR  | VERTEX_TEXCOORD                );
        ADD_VERTEX_TYPE(VERTEX_COLOR  | VERTEX_NORMAL                  );
        ADD_VERTEX_TYPE(VERTEX_NORMAL | VERTEX_TEXCOORD                );
        ADD_VERTEX_TYPE(VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD);

        #undef ADD_VERTEX_TYPE

        layouts[POSITION].begin().add(bgfx::Attrib::Position , 3, bgfx::AttribType::Float      ).end();
        layouts[COLOR   ].begin().add(bgfx::Attrib::Color0   , 4, bgfx::AttribType::Uint8, true).end();
        layouts[NORMAL  ].begin().add(bgfx::Attrib::Normal   , 3, bgfx::AttribType::Float      ).end();
        layouts[TEXCOORD].begin().add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float      ).end();
    }

private:
    template <int ATTRIBUTES>
    static void attrib_push_func(GeometryBuilder& builder);
};

template <typename T>
static bool  update_transient_buffer
(
    const std::vector<T>&        src,
    const bgfx::VertexLayout&    layout,
    bgfx::TransientVertexBuffer& dst
)
{
    const uint32_t size = static_cast<uint32_t>(src.size());

    if (size == 0)
    {
        dst = { nullptr, 0, 0, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
        return true;
    }

    if (size > bgfx::getAvailTransientVertexBuffer(size, layout))
    {
        assert(false);
        return false;
    }

    bgfx::allocTransientVertexBuffer(&dst, size, layout);
    memcpy(dst.data, src.data(), size * sizeof(T));

    return true;
}

struct GeometryBuilder
{
    // TODO : Will need texture recording as well.
    struct Primitive
    {
        int      attributes = 0;
        uint32_t count      = 0;
        uint32_t positions  = 0; // -+-- Offsets in respective arrays.
        uint32_t colors     = 0; //  |
        uint32_t normals    = 0; //  |
        uint32_t texcoords  = 0; // -+
    };

    static const VertexVariantInfo s_variants;

    bgfx::TransientVertexBuffer buffers[VertexVariantInfo::MAX_VERTEX_ATTRS];
    std::vector<Primitive>      primitives;
    std::vector<hmm_vec3>       positions;
    Stack<uint32_t>             colors;
    Stack<hmm_vec3>             normals;
    Stack<hmm_vec2>             texcoords;
    MatrixStack                 transforms = { HMM_Mat4d(1.0f) };
    int                         mode       = -1;

    inline void clear()
    {
        primitives    .clear();
        positions     .clear();
        colors   .data.clear();
        normals  .data.clear();
        texcoords.data.clear();
    }

    void begin(int attributes)
    {
        assert(attributes == (attributes & (VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD)));
        mode = attributes;

        if (primitives.empty() || primitives.back().attributes != attributes)
        {
            primitives.push_back(
            {
                attributes,
                static_cast<uint32_t>(positions     .size()),
                static_cast<uint32_t>(colors   .data.size()),
                static_cast<uint32_t>(normals  .data.size()),
                static_cast<uint32_t>(texcoords.data.size()),
            });
        }
    }

    void end()
    {
        assert(!primitives.empty() && primitives.back().count % 3 == 0);

        primitives.back().count = static_cast<uint32_t>(positions.size() - primitives.back().positions);
    }

    inline void vertex(float x, float y, float z)
    {
        assert(mode >= 0 && mode < VertexVariantInfo::MAX_VERTEX_TYPES);

        positions.push_back((transforms.top * HMM_Vec4(x, y, z, 1.0f)).XYZ);

        s_variants.funcs[mode](*this);
    }

    inline void color(unsigned int abgr)
    {
        colors.top = abgr;
    }

    inline void normal(float nx, float ny, float nz)
    {
        normals.top = (transforms.top * HMM_Vec4(nx, ny, nz, 0.0f)).XYZ;
    }

    inline void texcoord(float u, float v)
    {
        texcoords.top = HMM_Vec2(u, v);
    }

    // TODO : Separate program for every vertex type.
    bool submit(bgfx::ViewId view, bgfx::ProgramHandle program)
    {
        if (!update_transient_buffer(positions     , s_variants.layouts[VertexVariantInfo::POSITION], buffers[VertexVariantInfo::POSITION]) ||
            !update_transient_buffer(colors   .data, s_variants.layouts[VertexVariantInfo::COLOR   ], buffers[VertexVariantInfo::COLOR   ]) ||
            !update_transient_buffer(normals  .data, s_variants.layouts[VertexVariantInfo::NORMAL  ], buffers[VertexVariantInfo::NORMAL  ]) ||
            !update_transient_buffer(texcoords.data, s_variants.layouts[VertexVariantInfo::TEXCOORD], buffers[VertexVariantInfo::TEXCOORD]))
        {
            return false;
        }

        // TODO : Submit each primitive separately, with correct offsets.
        // TODO : This has to use the encoder, since it could be called from any thread.
        bgfx::setVertexBuffer(0, &buffers[VertexVariantInfo::POSITION]);
        bgfx::setVertexBuffer(1, &buffers[VertexVariantInfo::COLOR   ]);
        bgfx::setState(BGFX_STATE_DEFAULT);

        bgfx::submit(view, program);

        return true;
    }
};

const VertexVariantInfo GeometryBuilder::s_variants;

template <int ATTRIBUTES>
void VertexVariantInfo::attrib_push_func(GeometryBuilder& builder)
{
    if (ATTRIBUTES & VERTEX_COLOR)
    {
        builder.colors.push();
    }

    if (ATTRIBUTES & VERTEX_NORMAL)
    {
        builder.normals.push();
    }

    if (ATTRIBUTES & VERTEX_TEXCOORD)
    {
        builder.texcoords.push();
    }
}

struct Context
{
    GeometryBuilder     geometry;
    // std::vector<Vertex>  vertices;
    // Stack<Attribs>       attribs  = { 0xffffff };
    // MatrixStack          models   = { HMM_Mat4d(1.0f) };
    MatrixStack          views    = { HMM_Mat4d(1.0f) };
    MatrixStack          projs    = { HMM_Mat4d(1.0f) };
    MatrixStack*         matrices = &geometry.transforms;

    // TODO : Window and the timers don't need to be in the thread-local
    //        contexts, only the geometry builders do.
    GLFWwindow*          window   = nullptr;
    Timer                total;
    Timer                frame;
};

static Context& get_context()
{
    static thread_local Context s_ctx;
    return s_ctx;
}

static Keyboard& get_keyboard()
{
    static Keyboard s_keyboard;
    return s_keyboard;
}

static Mouse& get_mouse()
{
    static Mouse s_mouse;
    return s_mouse;
}

static enki::TaskScheduler& get_scheduler()
{
    static enki::TaskScheduler s_scheduler;
    return s_scheduler;
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
    glfwSetWindowTitle(get_context().window, title);
}

int width(void)
{
    int width;
    glfwGetWindowSize(get_context().window, &width, nullptr);

    return width;
}

int height(void)
{
    int height;
    glfwGetWindowSize(get_context().window, nullptr, &height);

    return height;
}

float aspect(void)
{
    int width, height;
    glfwGetWindowSize(get_context().window, &width, &height);

    return static_cast<float>(width) / static_cast<float>(height);
}

float dpi(void)
{
    int fb_width;
    glfwGetFramebufferSize(get_context().window, &fb_width, nullptr);

    int width;
    glfwGetWindowSize(get_context().window, &width, nullptr);

    return static_cast<float>(fb_width) / static_cast<float>(width);
}

void quit(void)
{
    glfwSetWindowShouldClose(get_context().window, GLFW_TRUE);
}

// static void submit_immediate_geometry
// (
//     bgfx::ViewId           view,
//     bgfx::ProgramHandle    program,
//     const GeometryBuilder& geometry
//     // const bgfx::VertexLayout&  layout,
//     // const std::vector<Vertex>& vertices
// )
// {
//     // TODO : This has to submit each sepearate vertex type.

//     assert(geometry.positions.size() % 3 == 0);

//     const uint32_t num_vertices = static_cast<uint32_t>(geometry.positions.size());

//     if (num_vertices > bgfx::getAvailTransientVertexBuffer(num_vertices, geometry.s_variants[]))
//     {
//         assert(false);
//         return;
//     }

//     bgfx::TransientVertexBuffer tvb;
//     bgfx::allocTransientVertexBuffer(&tvb, num_vertices, layout);
//     memcpy(tvb.data, vertices.data(), num_vertices * sizeof(Vertex));

//     bgfx::setVertexBuffer(0, &tvb);
//     bgfx::setState(BGFX_STATE_DEFAULT);
//     bgfx::submit(view, program);
// }

int mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    if (glfwInit() != GLFW_TRUE)
    {
        return 1;
    }

    gleqInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    Context&             ctx       = get_context  ();
    Keyboard&            keyboard  = get_keyboard ();
    Mouse&               mouse     = get_mouse    ();
    enki::TaskScheduler& scheduler = get_scheduler();

    ctx.window = glfwCreateWindow(640, 480, "MiNiMo", nullptr, nullptr);
    if (!ctx.window)
    {
        glfwTerminate();
        return 2;
    }

    gleqTrackWindow(ctx.window);

    bgfx::Init init;
    init.platformData = create_platform_data(ctx.window, init.type);

    if (!bgfx::init(init))
    {
        glfwDestroyWindow(ctx.window);
        glfwTerminate();
        return 3;
    }

    // Post a GLEQ_FRAMEBUFFER_RESIZED event, in case the user doesn't change
    // the window size.
    {
        int width  = 0;
        int height = 0;
        glfwGetFramebufferSize(ctx.window, &width, &height);

        gleq_framebuffer_size_callback(ctx.window, width, height);
    }

    scheduler.Initialize(std::max(3u, std::thread::hardware_concurrency()) - 1);

    if (setup)
    {
        (*setup)();
    }

    bgfx::setDebug(BGFX_DEBUG_STATS);

    int last_fb_width  = 0;
    int last_fb_height = 0;

    constexpr bgfx::ViewId DEFAULT_VIEW = 0;
    bgfx::setViewClear(DEFAULT_VIEW, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x333333ff);

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

    {
        double x, y;
        glfwGetCursorPos(ctx.window, &x, &y);

        mouse.curr[0] = mouse.prev[0] = static_cast<int>(x);
        mouse.curr[1] = mouse.prev[1] = static_cast<int>(y);
    }

    ctx.total.tic();
    ctx.frame.tic();

    while (!glfwWindowShouldClose(ctx.window))
    {
        keyboard.update_state_flags();
        mouse   .update_state_flags();

        ctx.total.toc();
        ctx.frame.toc(true);

        glfwPollEvents();

        GLEQevent event;
        while (gleqNextEvent(&event))
        {
            switch (event.type)
            {
            case GLEQ_KEY_PRESSED:
                keyboard.update_input_state(event.keyboard.key, true);
                break;
            
            case GLEQ_KEY_RELEASED:
                keyboard.update_input_state(event.keyboard.key, false);
                break;

            case GLEQ_BUTTON_PRESSED:
                mouse.update_input_state(event.mouse.button, true);
                break;

            case GLEQ_BUTTON_RELEASED:
                mouse.update_input_state(event.mouse.button, false);
                break;

            case GLEQ_CURSOR_MOVED:
                mouse.update_position(event.pos.x, event.pos.y);
                break;

            case GLEQ_FRAMEBUFFER_RESIZED:
                {
                    const uint16_t width  = static_cast<uint16_t>(event.size.width );
                    const uint16_t height = static_cast<uint16_t>(event.size.height);

                    bgfx::reset(width, height, BGFX_RESET_NONE);
                    bgfx::setViewRect (DEFAULT_VIEW, 0, 0, width, height);
                }
                break;

            default:;
            }

            gleqFreeEvent(&event);
        }

        // We have to wait with the mouse delta computation after all events
        // have been processed (there could be multiple `GLEQ_CURSOR_MOVED` ones).
        mouse.update_position_delta();

        bgfx::touch(DEFAULT_VIEW);

        // TODO : This needs to be done for all contexts across all threads.
        ctx.geometry.clear();

        if (draw)
        {
            (*draw)();
        }

        bgfx::setViewTransform(DEFAULT_VIEW, &ctx.views.top, &ctx.projs.top);

        // TODO : This needs to be done for all contexts across all threads.
        ctx.geometry.submit(DEFAULT_VIEW, program);
        // submit_immediate_geometry(DEFAULT_VIEW, program, ctx.geometry);

        bgfx::frame();
    }

    if (cleanup)
    {
        (*cleanup)();
    }

    scheduler.WaitforAllAndShutdown();

    bgfx::shutdown();

    glfwDestroyWindow(ctx.window);
    glfwTerminate();

    return 0;
}

void begin(void)
{
    get_context().geometry.begin(VERTEX_COLOR);
}

void end(void)
{
    get_context().geometry.end();
}

void vertex(float x, float y, float z)
{
    get_context().geometry.vertex(x, y, z);
}

void color(unsigned int rgba)
{
    get_context().geometry.color(bx::endianSwap(rgba));
}

// TODO : Move these into a separate compilation unit, so that.
// void normal(float nx, float ny, float nz)
// {
//     get_context().geometry.normal(nx, ny, nz);
// }

void texcoord(float u, float v)
{
    get_context().geometry.texcoord(u, v);
}

void model(void)
{
    Context& ctx = get_context();
    ctx.matrices = &ctx.geometry.transforms;
}

void view(void)
{
    Context& ctx = get_context();
    ctx.matrices = &ctx.views;
}

void projection(void)
{
    Context& ctx = get_context();
    ctx.matrices = &ctx.projs;
}

void push(void)
{
    // TODO : Push vertex attribs (if that's something desirable).
    get_context().geometry.transforms.push();
}

void pop(void)
{
    // TODO : Pop vertex attribs (if that's something desirable).
    get_context().geometry.transforms.pop();
}

void identity(void)
{
    get_context().matrices->top = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near_, float far_)
{
    get_context().matrices->mul(HMM_Orthographic(left, right, bottom, top, near_, far_));
}

void perspective(float fovy, float aspect, float near_, float far_)
{
    get_context().matrices->mul(HMM_Perspective(fovy, aspect, near_, far_));
}

void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y, float at_z, float up_x, float up_y, float up_z)
{
    get_context().matrices->mul(HMM_LookAt(HMM_Vec3(eye_x, eye_y, eye_z), HMM_Vec3(at_x, at_y, at_z), HMM_Vec3(up_x, up_y, up_z)));
}

void rotate(float angle, float x, float y, float z)
{
    get_context().matrices->mul(HMM_Rotate(angle, HMM_Vec3(x, y, x)));
}

void rotate_x(float angle)
{
    // TODO : General rotation matrix is wasteful here.
    rotate(angle, 1.0f, 0.0f, 0.0f);
}

void rotate_y(float angle)
{
    // TODO : General rotation matrix is wasteful here.
    rotate(angle, 0.0f, 1.0f, 0.0f);
}

void rotate_z(float angle)
{
    // TODO : General rotation matrix is wasteful here.
    rotate(angle, 0.0f, 0.0f, 1.0f);
}

void scale(float scale)
{
    get_context().matrices->mul(HMM_Scale(HMM_Vec3(scale, scale, scale)));
}

void translate(float x, float y, float z)
{
    get_context().matrices->mul(HMM_Translate(HMM_Vec3(x, y, z)));
}

int key_down(int key)
{
    return get_keyboard().is(key, Keyboard::DOWN);
}

int key_held(int key)
{
    return get_keyboard().is(key, Keyboard::HELD);
}

int key_up(int key)
{
    return get_keyboard().is(key, Keyboard::UP);
}

int mouse_x(void)
{
    return get_mouse().curr[0];
}

int mouse_y(void)
{
    return get_mouse().curr[1];
}

int mouse_dx(void)
{
    return get_mouse().delta[0];
}

int mouse_dy(void)
{
    return get_mouse().delta[1];
}

int mouse_down(int button)
{
    return get_mouse().is(button, Mouse::DOWN);
}

int mouse_held(int button)
{
    return get_mouse().is(button, Mouse::HELD);
}

int mouse_up(int button)
{
    return get_mouse().is(button, Mouse::UP);
}

double elapsed(void)
{
    return get_context().total.elapsed;
}

double dt(void)
{
    return get_context().frame.elapsed;
}

void sleep_for(double seconds)
{
    // TODO : Assert that we're not in the main thread.
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

struct TaskPool;

struct Task : enki::ITaskSet
{
    void    (*func)(void*) = nullptr;
    void*     data         = nullptr;
    TaskPool* pool         = nullptr;

    void ExecuteRange(enki::TaskSetPartition, uint32_t) override;
};

struct TaskPool
{
    static constexpr int MAX_TASKS = 64;

    std::mutex mutex;
    Task       tasks[MAX_TASKS];
    int        nexts[MAX_TASKS];
    size_t     head = 0;

    TaskPool()
    {
        for (int i = 0; i < MAX_TASKS; i++)
        {
            tasks[i].pool = this;
            nexts[i]      = i + 1;
        }
    }

    Task* get_free_task()
    {
        std::lock_guard<std::mutex> lock(mutex);

        Task* task = nullptr;

        if (head < MAX_TASKS)
        {
            const int i = head;

            task     = &tasks[i];
            head     =  nexts[i];
            nexts[i] = MAX_TASKS;
        }

        return task;
    }

    void release_task(const Task* task)
    {
        assert(task);
        assert(task >= &tasks[0] && task <= &tasks[MAX_TASKS - 1]);

        std::lock_guard<std::mutex> lock(mutex);

        const int i = static_cast<int>(task - &tasks[0]);

        tasks[i].func = nullptr;
        tasks[i].data = nullptr;
        nexts[i]      = head;
        head          = i;
    }
};

void Task::ExecuteRange(enki::TaskSetPartition, uint32_t)
{
    assert(func);
    (*func)(data);

    assert(pool);
    pool->release_task(this);
}

static TaskPool& get_task_pool()
{
    static TaskPool s_task_pool;
    return s_task_pool;
};

int task(void (* func)(void* data), void* data)
{
    Task* task = get_task_pool().get_free_task();

    if (task)
    {
        task->func = func;
        task->data = data;

        get_scheduler().AddTaskSetToPipe(task);
    }

    return task != nullptr;
}
