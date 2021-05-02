#include <mnm/mnm.h>

#include <assert.h>                    // assert
#include <stdint.h>                    // uint32_t
#include <string.h>                    // memcpy

#include <algorithm>                   // max, transform
#include <chrono>                      // duration
#include <functional>                  // hash
#include <mutex>                       // lock_guard, mutex
#include <thread>                      // this_thread
#include <unordered_map>               // unordered_map
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
#include <gleq.h>                      // gleq*

#if BX_PLATFORM_OSX
#   import <Cocoa/Cocoa.h>             // NSWindow
#   import <QuartzCore/CAMetalLayer.h> // CAMetalLayer
#endif

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>              // HMM_*, hmm_*

#include <TaskScheduler.h>             // ITaskSet, TaskScheduler, TaskSetPartition

#include <shaders/poscolor_fs.h>       // poscolor_fs
#include <shaders/poscolor_vs.h>       // poscolor_vs


// -----------------------------------------------------------------------------
// PLATFORM HELPERS
// -----------------------------------------------------------------------------

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


// -----------------------------------------------------------------------------
// WINDOW
// -----------------------------------------------------------------------------

struct Window
{
    GLFWwindow* handle    = nullptr;
    int         width     = 0;
    int         height    = 0;
    int         fb_width  = 0;
    int         fb_height = 0;
};

static void resize_window(GLFWwindow* window, int width, int height, int flags)
{
    assert(window);
    assert(flags >= 0);

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


// -----------------------------------------------------------------------------
// STACK
// -----------------------------------------------------------------------------

template <typename T>
struct Stack
{
    T              top;
    std::vector<T> data;

    inline void push()
    {
        data.push_back(top);
    }

    inline void pop()
    {
        top = data.back();
        data.pop_back();
    }
};

struct MatrixStack : Stack<hmm_mat4>
{
    MatrixStack()
    {
        top = HMM_Mat4d(1.0f);
    }

    void multiply_top(const hmm_mat4& matrix)
    {
        top = matrix * top;
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

template <>
inline void hash_combine<hmm_vec2>(size_t& seed, const hmm_vec2& value)
{
    hash_combine(seed, value.X);
    hash_combine(seed, value.Y);
}

template <>
inline void hash_combine<hmm_vec3>(size_t& seed, const hmm_vec3& value)
{
    hash_combine(seed, value.X);
    hash_combine(seed, value.Y);
    hash_combine(seed, value.Z);
}


// -----------------------------------------------------------------------------
// GEOMETRY RECORDING
// -----------------------------------------------------------------------------

struct GeometryRecord
{
    int      id              = 0;
    uint16_t attributes      = 0;

    uint16_t index_count     = 0;
    uint16_t index_offset    = 0;

    uint16_t vertex_count    = 0;
    uint16_t position_offset = 0;
    uint16_t color_offset    = 0;
    uint16_t normal_offset   = 0;
    uint16_t texcoord_offset = 0;
};

struct GeometryRecorder
{
    typedef void (*PushFunc)(GeometryRecorder&);

    typedef size_t (*VertexHashFunc)(const GeometryRecorder&, const GeometryRecord&, size_t);

    static const PushFunc s_push_active_attributes[8];

    static const VertexHashFunc s_hash_vertex[8];

    std::vector<GeometryRecord> records;
    std::vector<hmm_vec3>       positions;
    Stack<uint32_t>             colors    { 0xffffff };
    Stack<hmm_vec3>             normals   { HMM_Vec3(0.0f, 0.0f, 1.0f) };
    Stack<hmm_vec2>             texcoords { HMM_Vec2(0.0f, 0.0f) };

    void clear()
    {
        records       .clear();
        positions     .clear();
        colors   .data.clear();
        normals  .data.clear();
        texcoords.data.clear();
    }

    void begin(uint16_t attributes, int id = 0)
    {
        assert(attributes == (attributes & (VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD)));

        if (id || records.empty() || records.back().attributes != attributes)
        {
            GeometryRecord record;

            record.id              = id;
            record.attributes      = attributes;
            record.position_offset = static_cast<uint16_t>(positions     .size());
            record.color_offset    = static_cast<uint16_t>(colors   .data.size());
            record.normal_offset   = static_cast<uint16_t>(normals  .data.size());
            record.texcoord_offset = static_cast<uint16_t>(texcoords.data.size());

            records.push_back(record);
        }
    }

    inline void end()
    {
        assert(!records.empty());
        assert( records.back().vertex_count % 3 == 0);

        records.back().vertex_count = static_cast<uint16_t>(positions.size() - records.back().position_offset);
    }

    template <int ATTRIBUTES>
    static void push_active_attributes(GeometryRecorder& recorder)
    {
        static_assert(
            ATTRIBUTES >= 0 && ATTRIBUTES <= (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD),
            "Invalid vertex attributes."
        );

        if (ATTRIBUTES & VERTEX_COLOR)
        {
            recorder.colors.push();
        }

        if (ATTRIBUTES & VERTEX_NORMAL)
        {
            recorder.normals.push();
        }

        if (ATTRIBUTES & VERTEX_TEXCOORD)
        {
            recorder.texcoords.push();
        }
    }

    template <int ATTRIBUTES>
    static size_t hash_vertex
    (
        const GeometryRecorder& recorder,
        const GeometryRecord&   record,
        size_t                  i
    )
    {
        static_assert(
            ATTRIBUTES >= 0 && ATTRIBUTES <= (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD),
            "Invalid vertex attributes."
        );

        size_t hash = 0;
        hash_combine(hash, recorder.positions[record.position_offset + i]);

        if (ATTRIBUTES & VERTEX_COLOR)
        {
            hash_combine(hash, recorder.colors.data[record.color_offset + i]);
        }

        if (ATTRIBUTES & VERTEX_NORMAL)
        {
            hash_combine(hash, recorder.normals.data[record.normal_offset + i]);
        }

        if (ATTRIBUTES & VERTEX_TEXCOORD)
        {
            hash_combine(hash, recorder.texcoords.data[record.texcoord_offset + i]);
        }

        return hash;
    }

    inline void vertex(float x, float y, float z)
    {
        assert(!records.empty());

        positions.push_back(HMM_Vec3(x, y, z));

        // TODO : Check if having the attributes flags cached locally makes any performance difference.
        s_push_active_attributes[records.back().attributes](*this);
    }

    inline void color(unsigned int abgr)
    {
        colors.top = abgr;
    }

    inline void normal(float nx, float ny, float nz)
    {
        normals.top = HMM_Vec3(nx, ny, nz);
    }

    inline void texcoord(float u, float v)
    {
        texcoords.top = HMM_Vec2(u, v);
    }
};

const GeometryRecorder::PushFunc GeometryRecorder::s_push_active_attributes[8] =
{
    GeometryRecorder::push_active_attributes<0                                              >,
    GeometryRecorder::push_active_attributes<VERTEX_COLOR                                   >,
    GeometryRecorder::push_active_attributes<VERTEX_NORMAL                                  >,
    GeometryRecorder::push_active_attributes<VERTEX_TEXCOORD                                >,
    GeometryRecorder::push_active_attributes<VERTEX_COLOR  | VERTEX_TEXCOORD                >,
    GeometryRecorder::push_active_attributes<VERTEX_COLOR  | VERTEX_NORMAL                  >,
    GeometryRecorder::push_active_attributes<VERTEX_NORMAL | VERTEX_TEXCOORD                >,
    GeometryRecorder::push_active_attributes<VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD>,
};

const GeometryRecorder::VertexHashFunc GeometryRecorder::s_hash_vertex[8] =
{
    GeometryRecorder::hash_vertex<0                                              >,
    GeometryRecorder::hash_vertex<VERTEX_COLOR                                   >,
    GeometryRecorder::hash_vertex<VERTEX_NORMAL                                  >,
    GeometryRecorder::hash_vertex<VERTEX_TEXCOORD                                >,
    GeometryRecorder::hash_vertex<VERTEX_COLOR  | VERTEX_TEXCOORD                >,
    GeometryRecorder::hash_vertex<VERTEX_COLOR  | VERTEX_NORMAL                  >,
    GeometryRecorder::hash_vertex<VERTEX_NORMAL | VERTEX_TEXCOORD                >,
    GeometryRecorder::hash_vertex<VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD>,
};


// -----------------------------------------------------------------------------
// GEOMETRY INDEXING
// -----------------------------------------------------------------------------

static void index_geometry_record
(
    const GeometryRecord&   record,
    const GeometryRecorder& recorder,
    std::vector<uint16_t>&  out_indices,
    std::vector<uint16_t>&  out_vertex_map
)
{
    std::unordered_map<size_t, uint16_t>   vertex_hashes;
    const GeometryRecorder::VertexHashFunc hash_vertex = GeometryRecorder::s_hash_vertex[record.attributes];

    out_indices.resize(record.vertex_count);
    out_vertex_map.clear();

    for (size_t i = 0; i < out_indices.size(); i++)
    {
        const size_t vertex_hash = hash_vertex(recorder, record, i);
        const auto it = vertex_hashes.find(vertex_hash);

        if (it != vertex_hashes.end())
        {
            out_indices[i] = it->second;
        }
        else
        {
            const uint16_t idx = static_cast<uint16_t>(out_vertex_map.size());
            assert(idx == out_vertex_map.size());

            out_vertex_map.push_back(static_cast<uint16_t>(i));

            out_indices[i] = idx;

            vertex_hashes.insert({ vertex_hash, idx });
        }
    }
}


// -----------------------------------------------------------------------------
// GPU BUFFER UPDATE
// -----------------------------------------------------------------------------

static constexpr bgfx::TransientVertexBuffer EMPTY_TRANSIENT_BUFFER = { nullptr, 0, 0, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };

struct TransientBuffers
{
    bgfx::TransientVertexBuffer positions = EMPTY_TRANSIENT_BUFFER;
    bgfx::TransientVertexBuffer colors    = EMPTY_TRANSIENT_BUFFER;
    bgfx::TransientVertexBuffer normals   = EMPTY_TRANSIENT_BUFFER;
    bgfx::TransientVertexBuffer texcoords = EMPTY_TRANSIENT_BUFFER;
};

struct CachedBuffers
{
    bgfx::VertexBufferHandle positions = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle colors    = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle normals   = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle texcoords = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  indices   = BGFX_INVALID_HANDLE;
};

struct CachedSubmission
{
    int      id;
    hmm_mat4 transform;
};

typedef std::unordered_map<int, CachedBuffers> CachedBufferMap;

typedef std::vector<CachedSubmission> CachedSubmissionList;

struct VertexLayouts
{
    bgfx::VertexLayout positions;
    bgfx::VertexLayout colors;
    bgfx::VertexLayout normals;
    bgfx::VertexLayout texcoords;

    VertexLayouts()
    {
        positions.begin().add(bgfx::Attrib::Position , 3, bgfx::AttribType::Float      ).end();
        colors   .begin().add(bgfx::Attrib::Color0   , 4, bgfx::AttribType::Uint8, true).end();
        normals  .begin().add(bgfx::Attrib::Normal   , 3, bgfx::AttribType::Float      ).end();
        texcoords.begin().add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float      ).end();
    }
};

static bool update_cached_buffer(const uint16_t* data, uint32_t offset, uint32_t count, bgfx::IndexBufferHandle& out_buffer)
{
    // TODO : Refactor with the vertex-buffer-oriented overload.

    if (bgfx::isValid(out_buffer))
    {
        bgfx::destroy(out_buffer);
        out_buffer = BGFX_INVALID_HANDLE;
    }

    if (!(data && count))
    {
        return true;
    }

    const bgfx::Memory* memory = bgfx::copy(data + offset, static_cast<uint32_t>(count * sizeof(uint16_t)));
    assert(memory);

    out_buffer = bgfx::createIndexBuffer(memory);

    return bgfx::isValid(out_buffer);
}

template <typename T>
static bool update_cached_buffer
(
    const T*                     data,
    uint32_t                     offset,
    uint32_t                     size,
    const bgfx::VertexLayout&    layout,
    const std::vector<uint16_t>& vertex_map,
    bgfx::VertexBufferHandle&    out_buffer
)
{
    if (bgfx::isValid(out_buffer))
    {
        bgfx::destroy(out_buffer);
        out_buffer = BGFX_INVALID_HANDLE;
    }

    if (!(data && size))
    {
        return true;
    }

    // TODO : Measure whether a custom allocator (or batch allocation) would be more suitable.
    const bgfx::Memory* memory = nullptr;

    if (vertex_map.empty())
    {
        memory = bgfx::copy(data + offset, static_cast<uint32_t>(size * sizeof(T)));
        assert(memory);
    }
    else
    {
        memory = bgfx::alloc(static_cast<uint32_t>(vertex_map.size() * sizeof(T)));
        assert(memory);

        std::transform(
            vertex_map.begin(), vertex_map.end(),
            reinterpret_cast<T*>(memory->data),
            [&](uint16_t idx)
            {
                return data[offset + idx];
            });
    }

    out_buffer = bgfx::createVertexBuffer(memory, layout);

    return bgfx::isValid(out_buffer);
}

static bool update_cached_geometry
(
    const GeometryRecorder& recorder,
    const VertexLayouts&    layouts,
    CachedBufferMap&        out_buffer_map
)
{
    std::vector<uint16_t> indices;
    std::vector<uint16_t> vertex_map;

    for (const GeometryRecord& record : recorder.records)
    {
        assert(record.id           != 0);
        assert(record.vertex_count >  0);

        CachedBuffers& buffers = out_buffer_map[record.id];

        index_geometry_record(record, recorder, indices, vertex_map);

        if (!(
            update_cached_buffer(indices.data(), 0, static_cast<uint16_t>(indices.size()), buffers.indices) &&

            update_cached_buffer(recorder.positions     .data(), record.position_offset, record.vertex_count, layouts.positions, vertex_map, buffers.positions) &&
            update_cached_buffer(recorder.colors   .data.data(), record.color_offset   , record.vertex_count, layouts.colors   , vertex_map, buffers.colors   ) &&
            update_cached_buffer(recorder.normals  .data.data(), record.normal_offset  , record.vertex_count, layouts.normals  , vertex_map, buffers.normals  ) &&
            update_cached_buffer(recorder.texcoords.data.data(), record.texcoord_offset, record.vertex_count, layouts.texcoords, vertex_map, buffers.texcoords)
        ))
        {
            // TODO : We probably shouldn't dismiss all the buffers if a single one fails to upload.
            return false;
        }
    }

    // TODO : We could check if any of the buffer sets in `out_buffer_map` is empty and remove it, but
    // users shouldn't submit empty begin/end pairs in the first place. There will also be a dedicated
    // function to delete cached geometry.

    return true;
}

static void submit_cached_geometry
(
    const CachedSubmissionList& cached_submissions,
    const CachedBufferMap&      buffer_map,
    bgfx::ViewId                view,
    bgfx::ProgramHandle         program
)
{
    // TODO : Use BGFX encoder.
    // TODO : The state, view ID and program should be part of each `GeometryRecord`s.

    for (const CachedSubmission& submission : cached_submissions)
    {
        const auto it = buffer_map.find(submission.id);
        assert(it != buffer_map.end());

        if (it != buffer_map.end())
        {
            const CachedBuffers& buffers = it->second;

            // TODO : Use record's attributes and make function variants like in case of `GeometryRecorder::PushFunc`.
                                                    bgfx::setVertexBuffer(0, buffers.positions);
            if (bgfx::isValid(buffers.colors   )) { bgfx::setVertexBuffer(1, buffers.colors   ); }
            if (bgfx::isValid(buffers.normals  )) { bgfx::setVertexBuffer(2, buffers.normals  ); }
            if (bgfx::isValid(buffers.texcoords)) { bgfx::setVertexBuffer(3, buffers.texcoords); }
            if (bgfx::isValid(buffers.indices  )) { bgfx::setIndexBuffer (   buffers.indices  ); }

            bgfx::setTransform(submission.transform.Elements);

            bgfx::setState(BGFX_STATE_DEFAULT);

            bgfx::submit(view, program);
        }
    }
}

template <typename T>
static bool update_transient_buffer(const std::vector<T>& data, const bgfx::VertexLayout& layout, bgfx::TransientVertexBuffer& buffer)
{
    const uint32_t size = static_cast<uint32_t>(data.size());

    if (size == 0)
    {
        buffer = EMPTY_TRANSIENT_BUFFER;
        return true;
    }

    if (size > bgfx::getAvailTransientVertexBuffer(size, layout))
    {
        assert(false && "Not enough memory for the transient geometry.");
        return false;
    }

    bgfx::allocTransientVertexBuffer(&buffer, size, layout);
    memcpy(buffer.data, data.data(), size * sizeof(T));

    return true;
}

static inline bool update_transient_buffers
(
    const GeometryRecorder& recorder,
    const VertexLayouts&    layouts,
    TransientBuffers&       out_buffers
)
{
    return
        update_transient_buffer(recorder.positions     , layouts.positions, out_buffers.positions) &&
        update_transient_buffer(recorder.colors   .data, layouts.colors   , out_buffers.colors   ) &&
        update_transient_buffer(recorder.normals  .data, layouts.normals  , out_buffers.normals  ) &&
        update_transient_buffer(recorder.texcoords.data, layouts.texcoords, out_buffers.texcoords);
}

static void submit_transient_geometry
(
    const GeometryRecorder& recorder,
    const TransientBuffers& buffers,
    bgfx::ViewId            view,
    bgfx::ProgramHandle     program
)
{
    // TODO : Use BGFX encoder.
    // TODO : The state, view ID and program should be part of each `GeometryRecord`s.

    for (const GeometryRecord& record : recorder.records)
    {
        assert(record.vertex_count > 0);

        // TODO : Make function variants like in case of `GeometryRecorder::PushFunc`.
                                                   bgfx::setVertexBuffer(0, &buffers.positions);
        if (record.attributes & VERTEX_COLOR   ) { bgfx::setVertexBuffer(1, &buffers.colors   ); }
        if (record.attributes & VERTEX_NORMAL  ) { bgfx::setVertexBuffer(2, &buffers.normals  ); }
        if (record.attributes & VERTEX_TEXCOORD) { bgfx::setVertexBuffer(3, &buffers.texcoords); }

        bgfx::setState(BGFX_STATE_DEFAULT);

        bgfx::submit(view, program);
    }
}


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

    enki::TaskScheduler scheduler;
    TaskPool            task_pool;

    VertexLayouts       layouts;

    Timer               total_time;
    Timer               frame_time;

    Window              window;

    bool                vsync_on          = false;
    bool                reset_back_buffer = true;
};

struct ThreadContext
{
    GeometryRecorder     transient_recorder;
    GeometryRecorder     cached_recorder;

    MatrixStack          view_matrices;
    MatrixStack          proj_matrices;
    MatrixStack          model_matrices;

    TransientBuffers     transient_buffers;
    CachedBufferMap      cached_buffers;

    CachedSubmissionList cached_submissions;

    Timer                stop_watch;
    Timer                frame_time;

    GeometryRecorder*    recorder       = &transient_recorder;
    MatrixStack*         matrices       = &model_matrices;

    bool              is_main_thread = false;
};

static GlobalContext g_ctx;

static ThreadContext t_ctx;


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY
// -----------------------------------------------------------------------------

int mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    t_ctx.is_main_thread = true;

    if (glfwInit() != GLFW_TRUE)
    {
        return 1;
    }

    gleqInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    g_ctx.window.handle = glfwCreateWindow(640, 480, "MiNiMo", nullptr, nullptr);
    if (!g_ctx.window.handle)
    {
        glfwTerminate();
        return 2;
    }

    gleqTrackWindow(g_ctx.window.handle);

    bgfx::Init init;
    init.platformData = create_platform_data(g_ctx.window.handle, init.type);

    if (!bgfx::init(init))
    {
        glfwDestroyWindow(g_ctx.window.handle);
        glfwTerminate();
        return 3;
    }

    // Post size-related events, in case the user doesn't change the window size in the `setup` function.
    {
        int width  = 0;
        int height = 0;

        glfwGetWindowSize             (g_ctx.window.handle, &width, &height);
        gleq_window_size_callback     (g_ctx.window.handle,  width,  height);

        glfwGetFramebufferSize        (g_ctx.window.handle, &width, &height);
        gleq_framebuffer_size_callback(g_ctx.window.handle,  width,  height);
    }

    g_ctx.scheduler.Initialize(std::max(3u, std::thread::hardware_concurrency()) - 1);

    if (setup)
    {
        (*setup)();

        (void)update_cached_geometry(t_ctx.cached_recorder, g_ctx.layouts, t_ctx.cached_buffers);
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
        glfwGetCursorPos(g_ctx.window.handle, &x, &y);

        g_ctx.mouse.curr[0] = g_ctx.mouse.prev[0] = static_cast<int>(x);
        g_ctx.mouse.curr[1] = g_ctx.mouse.prev[1] = static_cast<int>(y);
    }

    g_ctx.total_time.tic();
    g_ctx.frame_time.tic();

    while (!glfwWindowShouldClose(g_ctx.window.handle))
    {
        g_ctx.keyboard.update_state_flags();
        g_ctx.mouse   .update_state_flags();

        g_ctx.total_time.toc();
        g_ctx.frame_time.toc(true);

        glfwPollEvents();

        GLEQevent event;
        while (gleqNextEvent(&event))
        {
            switch (event.type)
            {
            case GLEQ_KEY_PRESSED:
                g_ctx.keyboard.update_input_state(event.keyboard.key, true);
                break;
            
            case GLEQ_KEY_RELEASED:
                g_ctx.keyboard.update_input_state(event.keyboard.key, false);
                break;

            case GLEQ_BUTTON_PRESSED:
                g_ctx.mouse.update_input_state(event.mouse.button, true);
                break;

            case GLEQ_BUTTON_RELEASED:
                g_ctx.mouse.update_input_state(event.mouse.button, false);
                break;

            case GLEQ_CURSOR_MOVED:
                g_ctx.mouse.update_position(event.pos.x, event.pos.y);
                break;

            case GLEQ_FRAMEBUFFER_RESIZED:
                g_ctx.window.fb_width   = event.size.width;
                g_ctx.window.fb_height  = event.size.height;
                g_ctx.reset_back_buffer = true;
                break;

            case GLEQ_WINDOW_RESIZED:
                g_ctx.window.width  = event.size.width;
                g_ctx.window.height = event.size.height;
                break;

            default:;
            }

            gleqFreeEvent(&event);
        }

        // We have to wait with the mouse delta computation after all events
        // have been processed (there could be multiple `GLEQ_CURSOR_MOVED` ones).
        g_ctx.mouse.update_position_delta();

        if (g_ctx.reset_back_buffer)
        {
            g_ctx.reset_back_buffer = false;

            const uint16_t width  = static_cast<uint16_t>(g_ctx.window.fb_width );
            const uint16_t height = static_cast<uint16_t>(g_ctx.window.fb_height);

            const uint32_t vsync  = g_ctx.vsync_on ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

            bgfx::reset(width, height, BGFX_RESET_NONE | vsync);
            bgfx::setViewRect (DEFAULT_VIEW, 0, 0, width, height);
        }

        bgfx::touch(DEFAULT_VIEW);

        // TODO : This needs to be done for all contexts across all threads.
        t_ctx.transient_recorder.clear();
        t_ctx.cached_recorder   .clear();
        t_ctx.cached_submissions.clear();

        if (draw)
        {
            (*draw)();
        }

        bgfx::setViewTransform(DEFAULT_VIEW, &t_ctx.view_matrices.top, &t_ctx.proj_matrices.top);

        // TODO : This needs to be done for all contexts across all threads.
        {
            if (update_transient_buffers(t_ctx.transient_recorder, g_ctx.layouts, t_ctx.transient_buffers))
            {
                submit_transient_geometry(t_ctx.transient_recorder, t_ctx.transient_buffers, DEFAULT_VIEW, program);
            }

            if (update_cached_geometry(t_ctx.cached_recorder, g_ctx.layouts, t_ctx.cached_buffers))
            {
                submit_cached_geometry(t_ctx.cached_submissions, t_ctx.cached_buffers, DEFAULT_VIEW, program);
            }
        }

        bgfx::frame();
    }

    if (cleanup)
    {
        (*cleanup)();
    }

    g_ctx.scheduler.WaitforAllAndShutdown();

    bgfx::shutdown();

    glfwDestroyWindow(g_ctx.window.handle);
    glfwTerminate();

    return 0;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - WINDOW
// -----------------------------------------------------------------------------

void size(int width, int height, int flags)
{
    assert(t_ctx.is_main_thread);

    resize_window(g_ctx.window.handle, width, height, flags);
}

void title(const char* title)
{
    assert(t_ctx.is_main_thread);

    glfwSetWindowTitle(g_ctx.window.handle, title);
}

void vsync(int vsync)
{
    assert(t_ctx.is_main_thread);

    g_ctx.vsync_on          = static_cast<bool>(vsync);
    g_ctx.reset_back_buffer = true;
}

void quit(void)
{
    assert(t_ctx.is_main_thread);

    glfwSetWindowShouldClose(g_ctx.window.handle, GLFW_TRUE);
}

int width(void)
{
    return g_ctx.window.width;
}

int height(void)
{
    return g_ctx.window.height;
}

float aspect(void)
{
    return static_cast<float>(g_ctx.window.width) / static_cast<float>(g_ctx.window.height);
}

float dpi(void)
{
    return static_cast<float>(g_ctx.window.fb_width) / static_cast<float>(g_ctx.window.width);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - INPUT
// -----------------------------------------------------------------------------

int mouse_x(void)
{
    return g_ctx.mouse.curr[0];
}

int mouse_y(void)
{
    return g_ctx.mouse.curr[1];
}

int mouse_dx(void)
{
    return g_ctx.mouse.delta[0];
}

int mouse_dy(void)
{
    return g_ctx.mouse.delta[1];
}

int mouse_down(int button)
{
    return g_ctx.mouse.is(button, Mouse::DOWN);
}

int mouse_held(int button)
{
    return g_ctx.mouse.is(button, Mouse::HELD);
}

int mouse_up(int button)
{
    return g_ctx.mouse.is(button, Mouse::UP);
}

int key_down(int key)
{
    return g_ctx.keyboard.is(key, Keyboard::DOWN);
}

int key_held(int key)
{
    return g_ctx.keyboard.is(key, Keyboard::HELD);
}

int key_up(int key)
{
    return g_ctx.keyboard.is(key, Keyboard::UP);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TIME
// -----------------------------------------------------------------------------

double elapsed(void)
{
    return g_ctx.total_time.elapsed;
}

double dt(void)
{
    return g_ctx.frame_time.elapsed;
}

void sleep_for(double seconds)
{
    assert(!t_ctx.is_main_thread);

    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

void tic(void)
{
    t_ctx.stop_watch.tic();
}

double toc(void)
{
    return t_ctx.stop_watch.toc();
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - GEOMETRY
// -----------------------------------------------------------------------------

void begin(void)
{
    // TODO : Check recording is not already in progress.

    t_ctx.recorder = &t_ctx.transient_recorder;

    t_ctx.recorder->begin(VERTEX_COLOR); // TODO : Current attributes.
}

void begin_cached(int id)
{
    // TODO : Check recording is not already in progress.
    // TODO : Eventually, it'd be great to enable dynamic buffers (that change occasionally, but not every frame).

    assert(id != 0);

    t_ctx.recorder = &t_ctx.cached_recorder;

    t_ctx.recorder->begin(VERTEX_COLOR, id); // TODO : Current attributes.
}

void vertex(float x, float y, float z)
{
    const hmm_vec4 position = t_ctx.model_matrices.top * HMM_Vec4(x, y, z, 1.0f);

    t_ctx.recorder->vertex(position.X, position.Y, position.Z);
}

void color(unsigned int rgba)
{
    t_ctx.recorder->color(bx::endianSwap(rgba));
}

// TODO : Move these into a separate compilation unit, so that.
// void normal(float nx, float ny, float nz)
// {
//     t_ctx.recorder->normal(nx, ny, nz);
// }

void texcoord(float u, float v)
{
    t_ctx.recorder->texcoord(u, v);
}

void end(void)
{
    t_ctx.recorder->end();
}

void cache(int id)
{
    assert(id != 0);

    t_ctx.cached_submissions.push_back({ id, t_ctx.model_matrices.top });
}

void model(void)
{
    t_ctx.matrices = &t_ctx.model_matrices;
}

void view(void)
{
    t_ctx.matrices = &t_ctx.view_matrices;
}

void projection(void)
{
    t_ctx.matrices = &t_ctx.proj_matrices;
}

void push(void)
{
    // TODO : Push vertex attribs (if that's something desirable).
    t_ctx.matrices->push();
}

void pop(void)
{
    // TODO : Pop vertex attribs (if that's something desirable).
    t_ctx.matrices->pop();
}

void identity(void)
{
    t_ctx.matrices->top = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near_, float far_)
{
    t_ctx.matrices->multiply_top(HMM_Orthographic(left, right, bottom, top, near_, far_));
}

void perspective(float fovy, float aspect, float near_, float far_)
{
    t_ctx.matrices->multiply_top(HMM_Perspective(fovy, aspect, near_, far_));
}

void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y, float at_z, float up_x, float up_y, float up_z)
{
    t_ctx.matrices->multiply_top(HMM_LookAt(HMM_Vec3(eye_x, eye_y, eye_z), HMM_Vec3(at_x, at_y, at_z), HMM_Vec3(up_x, up_y, up_z)));
}

void rotate(float angle, float x, float y, float z)
{
    t_ctx.matrices->multiply_top(HMM_Rotate(angle, HMM_Vec3(x, y, x)));
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
    t_ctx.matrices->multiply_top(HMM_Scale(HMM_Vec3(scale, scale, scale)));
}

void translate(float x, float y, float z)
{
    t_ctx.matrices->multiply_top(HMM_Translate(HMM_Vec3(x, y, z)));
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MULTITHREADING
// -----------------------------------------------------------------------------

int task(void (* func)(void* data), void* data)
{
    Task* task = g_ctx.task_pool.get_free_task();

    if (task)
    {
        task->func = func;
        task->data = data;

        g_ctx.scheduler.AddTaskSetToPipe(task);
    }

    return task != nullptr;
}
