#include <assert.h>                    // assert
#include <stdint.h>                    // uint32_t
#include <string.h>                    // memcpy

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

struct Context
{
    std::vector<Vertex>  vertices;
    Stack<Attribs>       attribs  = { 0xffffff };
    MatrixStack          models   = { HMM_Mat4d(1.0f) };
    MatrixStack          views    = { HMM_Mat4d(1.0f) };
    MatrixStack          projs    = { HMM_Mat4d(1.0f) };
    MatrixStack*         matrices = &models;

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

    gleqInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    Context&  ctx      = get_context ();
    Keyboard& keyboard = get_keyboard();
    Mouse&    mouse    = get_mouse   ();

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

    {
        double x, y;
        glfwGetCursorPos(ctx.window, &x, &y);

        mouse.curr[0] = mouse.prev[0] = static_cast<int>(x);
        mouse.curr[1] = mouse.prev[1] = static_cast<int>(y);
    }

    ctx.total.tic();
    ctx.frame.tic();

    while (!glfwWindowShouldClose(ctx.window)/* && glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) != GLFW_PRESS*/)
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

            default:;
            }

            gleqFreeEvent(&event);
        }

        // We have to wait with the mouse delta computation after all events
        // have been processed (there could be multiple `GLEQ_CURSOR_MOVED` ones).
        mouse.update_position_delta();

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
        ctx.vertices.clear();

        if (draw)
        {
            (*draw)();
        }

        bgfx::setViewTransform(DEFAULT_VIEW, &ctx.views.top, &ctx.projs.top);

        // TODO : This needs to be done for all contexts across all threads.
        submit_immediate_geometry(DEFAULT_VIEW, program, layout, ctx.vertices);

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

    ctx.vertices.push_back({ (ctx.models.top * HMM_Vec4(x, y, z, 1.0f)).XYZ, ctx.attribs.top });
}

void color(unsigned int rgba)
{
    get_context().attribs.top.color = bx::endianSwap(rgba);
}

void model(void)
{
    Context& ctx = get_context();
    ctx.matrices = &ctx.models;
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
    Context& ctx = get_context();

    ctx.attribs . push();
    ctx.matrices->push();
}

void pop(void)
{
    Context& ctx = get_context();

    ctx.attribs . pop();
    ctx.matrices->pop();
}

void identity(void)
{
    get_context().matrices->top = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near, float far)
{
    get_context().matrices->mul(HMM_Orthographic(left, right, bottom, top, near, far));
}

void perspective(float fovy, float aspect, float near, float far)
{
    get_context().matrices->mul(HMM_Perspective(fovy, aspect, near, far));
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
