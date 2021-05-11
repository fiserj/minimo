#include <mnm/mnm.h>

#include <assert.h>               // assert
#include <stddef.h>               // size_t
#include <stdint.h>               // *int*_t
#include <string.h>               // memcpy

#include <algorithm>              // max, transform
#include <chrono>                 // duration
#include <functional>             // hash
#include <mutex>                  // lock_guard, mutex
#include <thread>                 // this_thread
#include <type_traits>            // alignment_of, is_trivial, is_standard_layout
#include <unordered_map>          // unordered_map
#include <vector>                 // vector

#include <bgfx/bgfx.h>            // bgfx::*
#include <bgfx/embedded_shader.h> // BGFX_EMBEDDED_SHADER*

#include <bx/bx.h>                // BX_COUNTOF
#include <bx/endian.h>            // endianSwap
#include <bx/pixelformat.h>       // packRg16S, packRgb8
#include <bx/timer.h>             // getHPCounter, getHPFrequency

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>           // glfw*

#define GLEQ_IMPLEMENTATION
#define GLEQ_STATIC
#include <gleq.h>                 // gleq*

#define HANDMADE_MATH_IMPLEMENTATION
#define HMM_STATIC
#include <HandmadeMath.h>         // HMM_*, hmm_*

#include <meshoptimizer.h>        // meshopt_generateVertexRemap

#include <TaskScheduler.h>        // ITaskSet, TaskScheduler, TaskSetPartition

namespace mnm
{

// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

static constexpr int MIN_WINDOW_SIZE      = 240;

static constexpr int DEFAULT_WINDOW_WIDTH = 640;

static constexpr int DEFAULT_WINDOW_HEIGHT= 480;


// -----------------------------------------------------------------------------
// UTILITY MACROS
// -----------------------------------------------------------------------------

#define ASSERT(cond) assert(cond)


// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T>
using Vector = std::vector<T>;

template <typename Key, typename T>
using HashMap = std::unordered_map<Key, T>;

using Mat4 = hmm_mat4;

using Vec2 = hmm_vec2;

using Vec3 = hmm_vec3;

using Vec4 = hmm_vec4;


// -----------------------------------------------------------------------------
// PLATFORM HELPERS
// -----------------------------------------------------------------------------

// Compiled separately mainly due to the name clash of `normal` function with
// an enum from MacTypes.h.
extern bgfx::PlatformData create_platform_data
(
    GLFWwindow*              window,
    bgfx::RendererType::Enum renderer
);


// -----------------------------------------------------------------------------
// STACK VARIANTS
// -----------------------------------------------------------------------------

template <typename T>
class Stack
{
public:
    inline Stack(const T& top = T())
        : m_top(top)
    {
    }

    inline void push()
    {
        m_data.push_back(m_top);
    }

    inline void pop()
    {
        m_top = m_data.back();
        m_data.pop_back();
    }

    inline void clear() { m_data.clear(); }

    inline size_t size() const { return m_data.size(); }

    inline T& top() { return m_top; }

    inline const T& top() const { return m_top; }

    inline const Vector<T>& data() const { return data; }

protected:
    T         m_top;
    Vector<T> m_data;
};

class MatrixStack : public Stack<Mat4>
{
public:
    inline MatrixStack()
        : Stack<Mat4>(HMM_Mat4d(1.0f))
    {}

    inline void multiply_top(const Mat4& matrix)
    {
        m_top = matrix * m_top;
    }
};


// -----------------------------------------------------------------------------
// GENERAL UTILITY FUNCTIONS
// -----------------------------------------------------------------------------

// https://stackoverflow.com/a/2595226
template <typename T>
inline void hash_combine(size_t& seed, const T& value)
{
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename HandleT>
inline void destroy_if_valid(HandleT& handle)
{
    if (bgfx::isValid(handle))
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }
}

template <typename T>
constexpr bool is_pod()
{
    // Since std::is_pod is being deprecated as of C++20.
    return std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}


// -----------------------------------------------------------------------------
// DRAW SUBMISSION
// -----------------------------------------------------------------------------

// TODO : Many members (if not all) can use smaller types.
struct DrawItem
{
    int type             = 0;
    int state            = 0;
    int pass_id          = 0;
    int program_id       = 0;
    int mesh_id          = 0;
    int texture_id       = 0;
    int vertex_layout_id = 0;
};


// -----------------------------------------------------------------------------
// VERTEX LAYOUT CACHE
// -----------------------------------------------------------------------------

class VertexLayoutCache
{
public:
    void add(uint32_t attribs)
    {
        ASSERT(attribs < UINT8_MAX);

        if (attribs < m_ids.size() && m_ids[attribs] != UINT8_MAX)
        {
            return;
        }

        bgfx::VertexLayout layout;

        layout.begin();
        layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);

        if (!!(attribs & VERTEX_COLOR))
        {
            layout.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
        }

        if (!!(attribs & VERTEX_NORMAL))
        {
            layout.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true);
        }

        if (!!(attribs & VERTEX_TEXCOORD))
        {
            layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true);
        }

        layout.end();

        m_layouts.push_back(layout);
        m_handles.push_back(bgfx::createVertexLayout(layout));

        const size_t idx = m_handles.size() - 1;

        if (attribs >= m_ids.size())
        {
            m_ids.resize(attribs + 1, UINT8_MAX);
        }

        return;
    }

    inline bgfx::VertexLayoutHandle layout_handle(uint32_t attribs) const
    {
        ASSERT(attribs < UINT8_MAX);
        ASSERT(attribs < m_ids.size());
        ASSERT(m_ids[attribs] != UINT8_MAX);

        return m_handles[m_ids[attribs]];
    }

    void clear()
    {
        for (bgfx::VertexLayoutHandle& handle : m_handles)
        {
            bgfx::destroy(handle);
        }

        m_layouts.clear();
        m_handles.clear();
    }

protected:
    Vector<uint8_t>                  m_ids;
    Vector<bgfx::VertexLayout>       m_layouts;
    Vector<bgfx::VertexLayoutHandle> m_handles;
};


// -----------------------------------------------------------------------------
// GEOMETRY RECORDING
// -----------------------------------------------------------------------------

struct VertexAttribs
{
    uint32_t color    = 0xffffffff;
    uint32_t texcoord = 0x00000000;
    uint32_t normal   = 0x00ff0000;
};

struct GeometryRecord
{
    int      user_id;
    uint32_t attribs;
    uint32_t byte_offset;
    uint32_t byte_length;
};

class GeometryRecorder
{
private:
    using VertexPushFunc = void (*)(GeometryRecorder&, const Vec3&);

public:
    void begin(int user_id, uint32_t attribs)
    {
        ASSERT(!m_recording);
        ASSERT(attribs <  BX_COUNTOF(ms_push_func_table));
        ASSERT(ms_push_func_table[attribs]);

        m_push_func = ms_push_func_table[attribs];

        GeometryRecord record;
        record.user_id          = user_id;
        record.attribs          = attribs;
        record.byte_offset      = static_cast<uint32_t>(m_buffer.size());
        record.byte_length      = 0;

        m_records.push_back(record);
        m_recording = true;
    }

    void end()
    {
        ASSERT( m_recording);
        ASSERT(!m_records.empty());

        m_recording = false;
    }

    void vertex(const Vec3& position)
    {
        ASSERT(m_recording);

        (*m_push_func)(*this, position);
    }

    inline void color(uint32_t rgba)
    {
        ASSERT(m_recording);

        m_attribs.color = bx::endianSwap(rgba);
    }

    inline void normal(const Vec3& normal)
    {
        ASSERT(m_recording);

        const float normalized[] =
        {
            normal.X * 0.5f + 0.5f,
            normal.Y * 0.5f + 0.5f,
            normal.Z * 0.5f + 0.5f,
        };

        bx::packRgb8(&m_attribs.normal, normalized);
    }

    inline void texcoord(const Vec2& texcoord)
    {
        ASSERT(m_recording);

        bx::packRg16S(&m_attribs.texcoord, texcoord.Elements);
    }

private:
    template <typename T>
    static inline void store_attrib(const T& attrib, uint8_t* buffer)
    {
        static_assert(is_pod<T>(), "Attribute type is not POD.");
        static_assert(std::alignment_of<T>::value == 4, "Non-standard attribute alignment.");

        *reinterpret_cast<T*>(buffer) = attrib;
        buffer += sizeof(T);
    }

    template <uint32_t Attribs>
    static constexpr size_t attribs_size()
    {
        size_t size = sizeof(Vec3);

        if (!!(Attribs & VERTEX_COLOR))
        {
            size += sizeof(VertexAttribs::color);
        }

        if (!!(Attribs & VERTEX_NORMAL))
        {
            size += sizeof(VertexAttribs::normal);
        }

        if (!!(Attribs & VERTEX_TEXCOORD))
        {
            size += sizeof(VertexAttribs::texcoord);
        }

        return size;
    }

    template <uint32_t Attribs, size_t Size = attribs_size<Attribs>()>
    static void push_vertex(GeometryRecorder& recorder, const Vec3& position)
    {
        const size_t offset = recorder.m_buffer.size();

        recorder.m_records.back().byte_length += Size;
        recorder.m_buffer.resize(offset + Size);

        uint8_t* buffer = recorder.m_buffer.data() + offset;

        store_attrib(position, buffer);

        if (!!(Attribs & VERTEX_COLOR))
        {
            store_attrib(recorder.m_attribs.color, buffer);
        }

        if (!!(Attribs & VERTEX_NORMAL))
        {
            store_attrib(recorder.m_attribs.normal, buffer);
        }

        if (!!(Attribs & VERTEX_TEXCOORD))
        {
            store_attrib(recorder.m_attribs.texcoord, buffer);
        }
    }

protected:
    VertexAttribs          m_attribs;
    Vector<GeometryRecord> m_records;
    Vector<uint8_t>        m_buffer;
    VertexPushFunc         m_push_func = nullptr;
    bool                   m_recording = false;

    static const VertexPushFunc ms_push_func_table[8];
};

const GeometryRecorder::VertexPushFunc GeometryRecorder::ms_push_func_table[8] =
{
    GeometryRecorder::push_vertex<0>,

    GeometryRecorder::push_vertex<VERTEX_COLOR   >,
    GeometryRecorder::push_vertex<VERTEX_NORMAL  >,
    GeometryRecorder::push_vertex<VERTEX_TEXCOORD>,
        
    GeometryRecorder::push_vertex<VERTEX_COLOR  | VERTEX_NORMAL  >,
    GeometryRecorder::push_vertex<VERTEX_COLOR  | VERTEX_TEXCOORD>,
    GeometryRecorder::push_vertex<VERTEX_NORMAL | VERTEX_TEXCOORD>,

    GeometryRecorder::push_vertex<VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD>,
};


// -----------------------------------------------------------------------------
// TIME MEASUREMENT
// -----------------------------------------------------------------------------

struct Timer 
{
    static const double s_frequency;

    int64_t counter = 0;
    double  elapsed = 0.0;

    void tic()
    {
        counter = bx::getHPCounter();
    }

    double toc(bool restart = false)
    {
        const int64_t now = bx::getHPCounter();

        elapsed = (now - counter) / s_frequency;

        if (restart)
        {
            counter = now;
        }

        return elapsed;
    }
};

const double Timer::s_frequency = static_cast<double>(bx::getHPFrequency());


// -----------------------------------------------------------------------------
// WINDOW
// -----------------------------------------------------------------------------

struct Window
{
    GLFWwindow* handle               = nullptr;

    float       display_scale_x      = 0.0f;
    float       display_scale_y      = 0.0f;

    float       position_scale_x     = 0.0f;
    float       position_scale_y     = 0.0f;

    float       dpi_invariant_width  = 0.0f;
    float       dpi_invariant_height = 0.0f;

    int         framebuffer_width    = 0;
    int         framebuffer_height   = 0;

    void update_size_info()
    {
        assert(handle);

        int window_width  = 0;
        int window_height = 0;

        glfwGetWindowSize        (handle, &window_width     , &window_height     );
        glfwGetFramebufferSize   (handle, &framebuffer_width, &framebuffer_height);
        glfwGetWindowContentScale(handle, &display_scale_x  , &display_scale_y   );

        adjust_dimension(display_scale_x, window_width , framebuffer_width , dpi_invariant_width , position_scale_x);
        adjust_dimension(display_scale_y, window_height, framebuffer_height, dpi_invariant_height, position_scale_y);
    }

private:
    static void adjust_dimension
    (
        float  scale,
        int    window_size,
        int    framebuffer_size,
        float& out_invariant_size,
        float& out_position_scale
    )
    {
        if (scale != 1.0 && window_size * scale != static_cast<float>(framebuffer_size))
        {
            out_invariant_size = framebuffer_size / scale;
            out_position_scale = 1.0f / scale;
        }
        else
        {
            out_invariant_size = static_cast<float>(window_size);
            out_position_scale = 1.0f;
        }
    }
};

static void resize_window(GLFWwindow* window, int width, int height, int flags)
{
    // TODO : The DEFAULT and MIN sizes should include the DPI scale.

    assert(window);
    assert(flags >= 0);

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

        if (width  <= MIN_WINDOW_SIZE) { width  = DEFAULT_WINDOW_WIDTH ; }
        if (height <= MIN_WINDOW_SIZE) { height = DEFAULT_WINDOW_HEIGHT; }

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
    if (width  <= MIN_WINDOW_SIZE) { width  = DEFAULT_WINDOW_WIDTH ; }
    if (height <= MIN_WINDOW_SIZE) { height = DEFAULT_WINDOW_HEIGHT; }

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


// -----------------------------------------------------------------------------
// INPUT
// -----------------------------------------------------------------------------

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
    float curr [2] = { 0.0f };
    float prev [2] = { 0.0f };
    float delta[2] = { 0.0f };

    void update_position(const Window& window)
    {
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window.handle, &x, &y);

        curr[0] = static_cast<float>(window.position_scale_x * x);
        curr[1] = static_cast<float>(window.position_scale_y * y);
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


// -----------------------------------------------------------------------------
// TASK POOL
// -----------------------------------------------------------------------------

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
    int        head = 0;

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


// -----------------------------------------------------------------------------
// CONTEXTS
// -----------------------------------------------------------------------------

struct GlobalContext
{
    Keyboard            keyboard;
    Mouse               mouse;

    enki::TaskScheduler task_scheduler;
    TaskPool            task_pool;

    VertexLayoutCache   vertex_layout_cache;

    Timer               total_time;
    Timer               frame_time;

    Window              window;
};

struct LocalContext
{
    GeometryRecorder  transient_recorder;
    GeometryRecorder  static_recorder;

    MatrixStack       view_matrix_stack;
    MatrixStack       proj_matrix_stack;
    MatrixStack       model_matrix_stack;

    GeometryRecorder* active_recorder     = &transient_recorder;
    MatrixStack*      active_matrix_stack = &model_matrix_stack;

    bool              is_recording        = false;
    bool              is_main_thread      = false;
};

static GlobalContext g_ctx;

thread_local LocalContext t_ctx;


// -----------------------------------------------------------------------------

} // namespace mnm


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - GEOMETRY
// -----------------------------------------------------------------------------

static void begin_recording(mnm::GeometryRecorder& recorder, int id, int attribs)
{
    ASSERT(id);
    ASSERT(attribs == (attribs & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)));

    ASSERT(!mnm::t_ctx.is_recording);
    mnm::t_ctx.is_recording = true;

    mnm::t_ctx.active_recorder = &recorder;
    mnm::t_ctx.active_recorder->begin(id, static_cast<uint32_t>(attribs));
}

void begin_transient(int id, int attribs)
{
    begin_recording(mnm::t_ctx.transient_recorder, id, attribs);
}

void begin_static(int id, int attribs)
{
    begin_recording(mnm::t_ctx.static_recorder, id, attribs);
}

void end(void)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.is_recording = false;

    mnm::t_ctx.active_recorder->end();
}

// ...


// -----------------------------------------------------------------------------
// !!! TEST
// -----------------------------------------------------------------------------

void TEST(void)
{
    // ...
}