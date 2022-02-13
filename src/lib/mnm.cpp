#include <mnm/mnm.h>

#include <assert.h>               // assert
#include <float.h>                // FLT_MAX
#include <math.h>                 // floorf, roundf
#include <stddef.h>               // ptrdiff_t, size_t
#include <stdint.h>               // *int*_t, UINT*_MAX
#include <stdio.h>                // fclose, fopen, fread, fseek, ftell, fwrite
#include <string.h>               // memcpy, memset, strcat, strcmp, strcpy, strlen, strrchr, _stricmp (Windows)

#include <algorithm>              // fill, min/max, sort, transform, unique
#include <atomic>                 // atomic
#include <array>                  // array
#include <chrono>                 // duration
#include <functional>             // hash
#include <mutex>                  // lock_guard, mutex
#include <thread>                 // this_thread
#include <type_traits>            // alignment_of, conditional, is_trivial, is_standard_layout
#include <unordered_map>          // unordered_map
#include <vector>                 // vector

#include <bx/platform.h>          // BX_PLATFORM_*

#if BX_PLATFORM_LINUX || BX_PLATFORM_OSX
#   include <strings.h>           // strcasecmp
#endif

#if BX_PLATFORM_WINDOWS
#   define strcasecmp _stricmp
#endif

#include <bx/bx.h>                // BX_ALIGN_DECL_16, BX_COUNTOF, BX_UNUSED, isPowerOf2

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4127);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);
#include <bgfx/bgfx.h>            // bgfx::*
#include <bgfx/embedded_shader.h> // BGFX_EMBEDDED_SHADER*
BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4365);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
#include <bx/debug.h>             // debugPrintf
#include <bx/endian.h>            // endianSwap
#include <bx/pixelformat.h>       // packRg16S, packRgb8
#include <bx/timer.h>             // getHPCounter, getHPFrequency
BX_PRAGMA_DIAGNOSTIC_POP();

#define GLFW_INCLUDE_NONE
BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);
#include <GLFW/glfw3.h>           // glfw*
BX_PRAGMA_DIAGNOSTIC_POP();

#define GLEQ_IMPLEMENTATION
#define GLEQ_STATIC
BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wnested-anon-types");
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5039);
#include <gleq.h>                 // gleq*
BX_PRAGMA_DIAGNOSTIC_POP();

#define HANDMADE_MATH_IMPLEMENTATION
#define HMM_STATIC
BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wmissing-field-initializers");
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wnested-anon-types");
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function");
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4505);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
#include <HandmadeMath.h>         // HMM_*, hmm_*
BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4365);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
#include <meshoptimizer.h>        // meshopt_*
BX_PRAGMA_DIAGNOSTIC_POP();

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wmissing-field-initializers");
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function");
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4365);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5045);
#include <stb_image.h>            // stbi_load, stbi_load_from_memory, stbi_image_free
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>      // stbi_write_*
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>        // stbrp_*
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>         // stbtt_*
BX_PRAGMA_DIAGNOSTIC_POP();

#include <TaskScheduler.h>        // ITaskSet, TaskScheduler, TaskSetPartition

#include <mnm_shaders.h>          // *_fs, *_vs

#include "common.h"

namespace mnm
{


// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

constexpr i32 DEFAULT_WINDOW_HEIGHT = 600;
constexpr i32 DEFAULT_WINDOW_WIDTH  = 800;
constexpr i32 MIN_WINDOW_SIZE       = 240;

constexpr u32 ATLAS_FREE            = 0x08000;
constexpr u32 ATLAS_MONOSPACED      = 0x00002;
constexpr u32 MESH_INVALID          = 0x00006;
constexpr u32 VERTEX_POSITION       = 0x00000;

// These have to be cross-checked against regular mesh flags (see later).
constexpr u32 INSTANCING_SUPPORTED  = 0x10000;
constexpr u32 SAMPLER_COLOR_R       = 0x20000;
constexpr u32 TEXT_MESH             = 0x40000;
constexpr u32 VERTEX_PIXCOORD       = 0x80000;


// -----------------------------------------------------------------------------
// FLAG MASKS AND SHIFTS
// -----------------------------------------------------------------------------

constexpr u16 MESH_TYPE_MASK         = MESH_STATIC | MESH_TRANSIENT | MESH_DYNAMIC | MESH_INVALID;
constexpr u16 MESH_TYPE_SHIFT        = 1;

constexpr u16 PRIMITIVE_TYPE_MASK    = PRIMITIVE_TRIANGLES | PRIMITIVE_QUADS | PRIMITIVE_TRIANGLE_STRIP |
                                       PRIMITIVE_LINES | PRIMITIVE_LINE_STRIP | PRIMITIVE_POINTS;
constexpr u16 PRIMITIVE_TYPE_SHIFT   = 4;

constexpr u16 TEXT_H_ALIGN_MASK      = TEXT_H_ALIGN_LEFT | TEXT_H_ALIGN_CENTER | TEXT_H_ALIGN_RIGHT;
constexpr u16 TEXT_H_ALIGN_SHIFT     = 4;
constexpr u16 TEXT_TYPE_MASK         = TEXT_STATIC | TEXT_TRANSIENT | TEXT_DYNAMIC;
constexpr u16 TEXT_V_ALIGN_MASK      = TEXT_V_ALIGN_BASELINE | TEXT_V_ALIGN_MIDDLE | TEXT_V_ALIGN_CAP_HEIGHT;
constexpr u16 TEXT_V_ALIGN_SHIFT     = 7;
constexpr u16 TEXT_Y_AXIS_MASK       = TEXT_Y_AXIS_UP | TEXT_Y_AXIS_DOWN;
constexpr u16 TEXT_Y_AXIS_SHIFT      = 10;

constexpr u16 TEXTURE_BORDER_MASK    = TEXTURE_MIRROR | TEXTURE_CLAMP;
constexpr u16 TEXTURE_BORDER_SHIFT   = 1;
constexpr u16 TEXTURE_FORMAT_MASK    = TEXTURE_R8 | TEXTURE_D24S8 | TEXTURE_D32F;
constexpr u16 TEXTURE_FORMAT_SHIFT   = 3;
constexpr u16 TEXTURE_SAMPLING_MASK  = TEXTURE_NEAREST;
constexpr u16 TEXTURE_SAMPLING_SHIFT = 0;
constexpr u16 TEXTURE_TARGET_MASK    = TEXTURE_TARGET;
constexpr u16 TEXTURE_TARGET_SHIFT   = 6;

constexpr u16 VERTEX_ATTRIB_MASK     = VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD;
constexpr u16 VERTEX_ATTRIB_SHIFT    = 7; // VERTEX_COLOR => 1 (so that VERTEX_POSITION is zero)

constexpr u32 USER_MESH_FLAGS        = MESH_TYPE_MASK | PRIMITIVE_TYPE_MASK | VERTEX_ATTRIB_MASK | TEXCOORD_F32 |
                                       OPTIMIZE_GEOMETRY | NO_VERTEX_TRANSFORM | KEEP_CPU_GEOMETRY;
constexpr u32 INTERNAL_MESH_FLAGS    = INSTANCING_SUPPORTED | SAMPLER_COLOR_R | TEXT_MESH | VERTEX_PIXCOORD;

static_assert(
    0 == (INTERNAL_MESH_FLAGS & USER_MESH_FLAGS),
    "Internal mesh flags interfere with the user-exposed ones."
);

static_assert(
    bx::isPowerOf2(PRIMITIVE_QUADS),
    "`PRIMITIVE_QUADS` must be power of two."
);


// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T, uint32_t Size>
using Array = std::array<T, Size>;

template <typename T>
using Atomic = std::atomic<T>;

using Mutex = std::mutex;

using MutexScope = std::lock_guard<std::mutex>;

template <typename Key, typename T>
using HashMap = std::unordered_map<Key, T>;

template <typename T>
using Vector = std::vector<T>;

using Mat4 = hmm_mat4;

using Vec2 = hmm_vec2;

using Vec3 = hmm_vec3;

using Vec4 = hmm_vec4;


// -----------------------------------------------------------------------------
// DEFERRED EXECUTION
// -----------------------------------------------------------------------------

template <typename Func>
struct Deferred
{
    Func func;

    Deferred(const Deferred&) = delete;

    Deferred& operator=(const Deferred&) = delete;

    Deferred(Func&& func) : func(static_cast<Func&&>(func))
    {
    }

    ~Deferred()
    {
        func();
    }
};

template <typename Func>
Deferred<Func> make_deferred(Func&& func)
{
    return Deferred<Func>(static_cast<decltype(func)>(func));
}

#define defer(...) auto BX_CONCATENATE(deferred_ , __LINE__) = \
    make_deferred([&]() mutable { __VA_ARGS__; })


// -----------------------------------------------------------------------------
// LOGGING
// -----------------------------------------------------------------------------

// Taken almost verbatim from `bx_p.h`.

#define _MNM_TRACE(format, ...) \
    BX_MACRO_BLOCK_BEGIN \
        bx::debugPrintf(__FILE__ "(" BX_STRINGIZE(__LINE__) "): MiNiMo " format "\n", ##__VA_ARGS__); \
    BX_MACRO_BLOCK_END

#ifndef MNM_CONFIG_DEBUG
#   ifdef NDEBUG
#      define MNM_CONFIG_DEBUG 0
#   else
#      define MNM_CONFIG_DEBUG 1
#   endif
#endif

#if MNM_CONFIG_DEBUG
    #define MNM_TRACE _MNM_TRACE
#else
#   define MNM_TRACE(...) BX_NOOP()
#endif // MNM_CONFIG_DEBUG


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

template <typename T>
constexpr bool is_pod()
{
    // Since std::is_pod is being deprecated as of C++20.
    return std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}

static inline bool is_aligned(const void* ptr, size_t alignment)
{
    return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

template <size_t Size>
static inline void assign(const void* src, void* dst)
{
    struct Block
    {
        uint8_t bytes[Size];
    };

    ASSERT(is_aligned(src, std::alignment_of<Block>::value));
    ASSERT(is_aligned(dst, std::alignment_of<Block>::value));

    *static_cast<Block*>(dst) = *static_cast<const Block*>(src);
}

template <size_t Size>
static inline void push_back(Vector<uint8_t>& buffer, const void* data)
{
    static_assert(Size > 0, "Size must be positive.");

    buffer.resize(buffer.size() + Size);

    assign<Size>(data, buffer.data() + buffer.size() - Size);
}

static inline void push_back(Vector<uint8_t>& buffer, const void* data, size_t size)
{
    buffer.resize(buffer.size() + size);

    (void)memcpy(buffer.data() + buffer.size() - size, data, size);
}

template <typename T>
static inline void push_back(Vector<uint8_t>& buffer, const T& value)
{
    push_back<sizeof(T)>(buffer, &value);
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


// -----------------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------------

static inline uint16_t mesh_type(uint32_t flags)
{
    constexpr uint16_t types[] =
    {
        MESH_STATIC,
        MESH_TRANSIENT,
        MESH_DYNAMIC,
        MESH_INVALID,
    };

    return types[((flags & MESH_TYPE_MASK) >> MESH_TYPE_SHIFT)];
}

// NOTE : Currently unused.
// static inline uint16_t mesh_primitive(uint32_t flags)
// {
//     constexpr uint16_t primitives[] =
//     {
//         PRIMITIVE_TRIANGLES,
//         PRIMITIVE_QUADS,
//         PRIMITIVE_TRIANGLE_STRIP,
//         PRIMITIVE_LINES,
//         PRIMITIVE_LINE_STRIP,
//         PRIMITIVE_POINTS,
//     };

//     return primitives[((flags & PRIMITIVE_TYPE_MASK) >> PRIMITIVE_TYPE_SHIFT)];
// }

static inline uint16_t mesh_attribs(uint32_t flags)
{
    return (flags & VERTEX_ATTRIB_MASK);
}


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
// UNIFORMS
// -----------------------------------------------------------------------------

struct DefaultUniforms
{
    bgfx::UniformHandle color_texture_rgba = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle color_texture_r    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle texture_size       = BGFX_INVALID_HANDLE;

    void init()
    {
        color_texture_rgba = bgfx::createUniform("s_tex_color_rgba", bgfx::UniformType::Sampler);
        color_texture_r    = bgfx::createUniform("s_tex_color_r"   , bgfx::UniformType::Sampler);
        texture_size       = bgfx::createUniform("u_tex_size"      , bgfx::UniformType::Vec4   );
    }

    void clear()
    {
        destroy_if_valid(color_texture_rgba);
        destroy_if_valid(color_texture_r   );
        destroy_if_valid(texture_size      );
    }

    inline bgfx::UniformHandle default_sampler(bgfx::TextureFormat::Enum format) const
    {
        switch (format)
        {
        case bgfx::TextureFormat::RGBA8:
            return color_texture_rgba;
        case bgfx::TextureFormat::R8:
            return color_texture_r;
        default:
            return BGFX_INVALID_HANDLE;
        }
    }
};

struct Uniform
{
    bgfx::UniformHandle handle = BGFX_INVALID_HANDLE;

    void destroy()
    {
        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
            *this = {};
        }
    }
};

class UniformCache
{
public:
    void clear()
    {
        MutexScope lock(m_mutex);

        for (Uniform& uniform : m_uniforms)
        {
            uniform.destroy();
        }
    }

    bool add(uint16_t id, uint16_t type, uint16_t count, const char* name)
    {
        constexpr bgfx::UniformType::Enum types[] =
        {
            bgfx::UniformType::Count,

            bgfx::UniformType::Vec4,
            bgfx::UniformType::Mat4,
            bgfx::UniformType::Mat3,
            bgfx::UniformType::Sampler,
        };

        bgfx::UniformHandle handle = bgfx::createUniform(name, types[type], count);
        if (!bgfx::isValid( handle))
        {
            ASSERT(false && "Invalid uniform handle.");
            return false;
        }

        MutexScope lock(m_mutex);

        Uniform& uniform = m_uniforms[id];
        uniform.destroy();
        uniform.handle = handle;

        return true;
    }

    inline Uniform& operator[](uint16_t id) { return m_uniforms[id]; }

private:
    Mutex                        m_mutex;
    Array<Uniform, MAX_UNIFORMS> m_uniforms;
};


// -----------------------------------------------------------------------------
// DRAW STATE
// -----------------------------------------------------------------------------

struct InstanceData;

struct DrawState
{
    Mat4                     transform       = HMM_Mat4d(1.0f);
    const InstanceData*      instances       = nullptr;
    uint32_t                 element_start   = 0;
    uint32_t                 element_count   = UINT32_MAX;
    bgfx::ViewId             pass            = UINT16_MAX;
    bgfx::FrameBufferHandle  framebuffer     = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      program         = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle      texture         = BGFX_INVALID_HANDLE; // TODO : More texture slots.
    bgfx::UniformHandle      sampler         = BGFX_INVALID_HANDLE;
    uint16_t                 texture_size[2] = { 0, 0 };
    bgfx::VertexLayoutHandle vertex_alias    = BGFX_INVALID_HANDLE;
    uint16_t                 flags           = STATE_DEFAULT;
    uint8_t                  _pad[10];
};


// -----------------------------------------------------------------------------
// PROGRAM CACHE
// -----------------------------------------------------------------------------

class ProgramCache
{
public:
    ProgramCache()
    {
        m_handles .fill(BGFX_INVALID_HANDLE);
        m_builtins.fill(BGFX_INVALID_HANDLE);
    }

    void clear()
    {
        MutexScope lock(m_mutex);

        for (bgfx::ProgramHandle& handle : m_handles)
        {
            destroy_if_valid(handle);
        }

        for (bgfx::ProgramHandle& handle : m_builtins)
        {
            destroy_if_valid(handle);
        }
    }

    bool add(uint16_t id, bgfx::ShaderHandle vertex, bgfx::ShaderHandle fragment, uint32_t attribs = UINT32_MAX)
    {
        bgfx::ProgramHandle program = bgfx::createProgram(vertex, fragment, true);
        if (!bgfx::isValid( program))
        {
            ASSERT(false && "Invalid program handle.");
            return false;
        }

        MutexScope lock(m_mutex);

        ASSERT(!(id == UINT16_MAX && attribs != UINT32_MAX) || !bgfx::isValid(m_builtins[get_index_from_attribs(attribs)]));

        bgfx::ProgramHandle& handle = (id == UINT16_MAX && attribs != UINT32_MAX)
            ? m_builtins[get_index_from_attribs(attribs)]
            : m_handles[id];

        destroy_if_valid(handle);
        handle = program;

        return true;
    }

    bool add(uint16_t id, const bgfx::EmbeddedShader* shaders, bgfx::RendererType::Enum renderer, const char* vertex_name, const char* fragment_name, uint32_t attribs = UINT32_MAX)
    {
        bgfx::ShaderHandle vertex = bgfx::createEmbeddedShader(shaders, renderer, vertex_name);
        if (!bgfx::isValid(vertex))
        {
            ASSERT(false && "Invalid vertex shader handle.");
            return false;
        }

        bgfx::ShaderHandle fragment = bgfx::createEmbeddedShader(shaders, renderer, fragment_name);
        if (!bgfx::isValid(fragment))
        {
            ASSERT(false && "Invalid fragment shader handle.");
            bgfx::destroy(vertex);
            return false;
        }

        return add(id, vertex, fragment, attribs);
    }

    bool add(uint16_t id, const void* vertex_data, uint32_t vertex_size, const void* fragment_data, uint32_t fragment_size, uint32_t attribs = UINT32_MAX)
    {
        bgfx::ShaderHandle vertex = bgfx::createShader(bgfx::copy(vertex_data, vertex_size));
        if (!bgfx::isValid(vertex))
        {
            ASSERT(false && "Invalid vertex shader handle.");
            return false;
        }

        bgfx::ShaderHandle fragment = bgfx::createShader(bgfx::copy(fragment_data, fragment_size));
        if (!bgfx::isValid(fragment))
        {
            ASSERT(false && "Invalid fragment shader handle.");
            bgfx::destroy(vertex);
            return false;
        }

        return add(id, vertex, fragment, attribs);
    }

    inline bgfx::ProgramHandle operator[](uint16_t id) const
    {
        return m_handles[id];
    }

    inline bgfx::ProgramHandle builtin(uint32_t attribs) const
    {
        return m_builtins[get_index_from_attribs(attribs)];
    }

private:
    static inline constexpr uint16_t get_index_from_attribs(uint32_t attribs)
    {
        static_assert(
            VERTEX_ATTRIB_MASK   >> VERTEX_ATTRIB_SHIFT == 0b000111 &&
            INSTANCING_SUPPORTED >> 13                  == 0b001000 &&
            SAMPLER_COLOR_R      >> 13                  == 0b010000 &&
            VERTEX_PIXCOORD      >> 14                  == 0b100000,
            "Invalid index assumptions in `ProgramCache::get_index_from_attribs`."
        );

        return
            ((attribs & VERTEX_ATTRIB_MASK  ) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
            ((attribs & INSTANCING_SUPPORTED) >> 13                 ) | // Bit 3.
            ((attribs & SAMPLER_COLOR_R     ) >> 13                 ) | // Bit 4.
            ((attribs & VERTEX_PIXCOORD     ) >> 14                 ) ; // Bit 5.
    }

private:
    static constexpr uint32_t                MAX_BUILTINS = 64;

    Mutex                                    m_mutex;
    Array<bgfx::ProgramHandle, MAX_PROGRAMS> m_handles;
    Array<bgfx::ProgramHandle, MAX_BUILTINS> m_builtins;
};


// -----------------------------------------------------------------------------
// PASSES
// -----------------------------------------------------------------------------

class Pass
{
public:
    inline void update(bgfx::ViewId id, bgfx::Encoder* encoder, bool backbuffer_size_changed)
    {
        if (m_dirty_flags & DIRTY_TOUCH)
        {
            encoder->touch(id);
        }

        if (m_dirty_flags & DIRTY_CLEAR)
        {
            bgfx::setViewClear(id, m_clear_flags, m_clear_rgba, m_clear_depth, m_clear_stencil);
        }

        if (m_dirty_flags & DIRTY_TRANSFORM)
        {
            bgfx::setViewTransform(id, &m_view_matrix, &m_proj_matrix);
        }

        if ((m_dirty_flags & DIRTY_RECT) || (backbuffer_size_changed && m_viewport_width >= SIZE_EQUAL))
        {
            if (m_viewport_width >= SIZE_EQUAL)
            {
                bgfx::setViewRect(id, m_viewport_x, m_viewport_y, static_cast<bgfx::BackbufferRatio::Enum>(m_viewport_width - SIZE_EQUAL));
            }
            else
            {
                bgfx::setViewRect(id, m_viewport_x, m_viewport_y, m_viewport_width, m_viewport_height);
            }
        }

        if ((m_dirty_flags & DIRTY_FRAMEBUFFER) || backbuffer_size_changed)
        {
            // Having `BGFX_INVALID_HANDLE` here is OK.
            bgfx::setViewFrameBuffer(id, m_framebuffer);
        }

        m_dirty_flags = DIRTY_NONE;
    }

    inline void touch()
    {
        m_dirty_flags |= DIRTY_TOUCH;
    }

    inline void set_view(const Mat4& matrix)
    {
        m_view_matrix  = matrix;
        m_dirty_flags |= DIRTY_TRANSFORM;
    }

    inline void set_projection(const Mat4& matrix)
    {
        m_proj_matrix  = matrix;
        m_dirty_flags |= DIRTY_TRANSFORM;
    }

    inline void set_framebuffer(bgfx::FrameBufferHandle framebuffer)
    {
        m_framebuffer  = framebuffer;
        m_dirty_flags |= DIRTY_FRAMEBUFFER;
    }

    void set_no_clear()
    {
        if (m_clear_flags != BGFX_CLEAR_NONE)
        {
            m_clear_flags  = BGFX_CLEAR_NONE;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    void set_clear_depth(float depth)
    {
        if (m_clear_depth != depth || !(m_dirty_flags & BGFX_CLEAR_DEPTH) || !(m_clear_flags & BGFX_CLEAR_DEPTH))
        {
            m_clear_flags |= BGFX_CLEAR_DEPTH;
            m_clear_depth  = depth;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    void set_clear_color(uint32_t rgba)
    {
        if (m_clear_rgba != rgba || !(m_dirty_flags & BGFX_CLEAR_COLOR) || !(m_clear_flags & BGFX_CLEAR_COLOR))
        {
            m_clear_flags |= BGFX_CLEAR_COLOR;
            m_clear_rgba   = rgba;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    inline void set_viewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
    {
        ASSERT(width < SIZE_EQUAL || width == height);

        if (m_viewport_x      != x     ||
            m_viewport_y      != y     ||
            m_viewport_width  != width ||
            m_viewport_height != height)
        {
            m_viewport_x      = x;
            m_viewport_y      = y;
            m_viewport_width  = width;
            m_viewport_height = height;
            m_dirty_flags    |= DIRTY_RECT;
        }
    }

    inline bgfx::FrameBufferHandle framebuffer() const { return m_framebuffer; }

private:
    enum : uint8_t
    {
        DIRTY_NONE        = 0x00,
        DIRTY_CLEAR       = 0x01,
        DIRTY_TOUCH       = 0x02,
        DIRTY_TRANSFORM   = 0x04,
        DIRTY_RECT        = 0x08,
        DIRTY_FRAMEBUFFER = 0x10,
    };

private:
    Mat4                    m_view_matrix     = HMM_Mat4d(1.0f);
    Mat4                    m_proj_matrix     = HMM_Mat4d(1.0f);

    uint16_t                m_viewport_x      = 0;
    uint16_t                m_viewport_y      = 0;
    uint16_t                m_viewport_width  = SIZE_EQUAL;
    uint16_t                m_viewport_height = SIZE_EQUAL;

    bgfx::FrameBufferHandle m_framebuffer     = BGFX_INVALID_HANDLE;

    uint16_t                m_clear_flags     = BGFX_CLEAR_NONE;
    float                   m_clear_depth     = 1.0f;
    uint32_t                m_clear_rgba      = 0x000000ff;
    uint8_t                 m_clear_stencil   = 0;

    uint8_t                 m_dirty_flags     = DIRTY_CLEAR;
};

class PassCache
{
public:
    void update(bgfx::Encoder* encoder)
    {
        MutexScope lock(m_mutex);

        for (bgfx::ViewId id = 0; id < m_passes.size(); id++)
        {
            m_passes[id].update(id, encoder, m_backbuffer_size_changed);
        }

        m_backbuffer_size_changed = false;
    }

    void notify_backbuffer_size_changed()
    {
        m_backbuffer_size_changed = true;
    }

    // Changing pass properties directly is not thread safe, but it seems
    // super silly to actually attempt to do so from multiple threads.

    inline Pass& operator[](bgfx::ViewId i) { return m_passes[i]; }

    inline const Pass& operator[](bgfx::ViewId i) const { return m_passes[i]; }

private:
    Mutex                   m_mutex;
    Array<Pass, MAX_PASSES> m_passes;
    bool                    m_backbuffer_size_changed = true;
};


// -----------------------------------------------------------------------------
// VERTEX LAYOUT CACHE
// -----------------------------------------------------------------------------

// TODO : Add support for user defined layouts.
class VertexLayoutCache
{
public:
    VertexLayoutCache()
    {
        m_handles.fill(BGFX_INVALID_HANDLE);
    }

    // NOTE : Can't be done in constructor, as we need BGFX to be initialized.
    void init()
    {
        //  +-------------------------- VERTEX_COLOR
        //  |  +----------------------- VERTEX_NORMAL
        //  |  |  +-------------------- VERTEX_TEXCOORD
        //  |  |  |
        add<0, 0, 0>();
        add<1, 0, 0>();
        add<0, 1, 0>();
        add<0, 0, 1>();
        add<1, 1, 0>();
        add<1, 0, 1>();
        add<0, 1, 1>();
        add<1, 1, 1>();
    }

    inline void clear()
    {
        for (bgfx::VertexLayoutHandle& handle : m_handles)
        {
            destroy_if_valid(handle);
        }
    }

    inline void add(uint32_t flags)
    {
        constexpr uint32_t ATTRIB_MASK = VERTEX_ATTRIB_MASK | TEXCOORD_F32;
        
        add(flags & ATTRIB_MASK, 0);
    }

    bgfx::VertexLayoutHandle resolve_alias(uint32_t& inout_flags, uint32_t alias_flags)
    {
        const uint32_t orig_attribs  = mesh_attribs(inout_flags);
        const uint32_t alias_attribs = mesh_attribs(alias_flags);

        const uint32_t skips = orig_attribs & (~alias_attribs);
        const uint32_t idx   = get_index_from_flags(orig_attribs, skips);

        inout_flags &= ~skips;

        return m_handles[idx];
    }

    inline const bgfx::VertexLayout& operator[](uint32_t flags) const
    {
        return m_layouts[get_index_from_flags(flags)];
    }

private:
    static inline constexpr uint32_t get_index_from_flags(uint32_t attribs, uint32_t skips = 0)
    {
        static_assert(
            VERTEX_ATTRIB_MASK >>  VERTEX_ATTRIB_SHIFT       == 0b0000111 &&
           (VERTEX_ATTRIB_MASK >> (VERTEX_ATTRIB_SHIFT - 3)) == 0b0111000 &&
            TEXCOORD_F32       >>  6                         == 0b1000000,
            "Invalid index assumptions in `VertexLayoutCache::get_index_from_attribs`."
        );

        return
            ((skips   & VERTEX_ATTRIB_MASK) >>  VERTEX_ATTRIB_SHIFT     ) | // Bits 0..2.
            ((attribs & VERTEX_ATTRIB_MASK) >> (VERTEX_ATTRIB_SHIFT - 3)) | // Bits 3..5.
            ((attribs & TEXCOORD_F32      ) >>  6                       ) ; // Bit 6.
    }

    template <
        bool HasColor,
        bool HasNormal,
        bool HasTexCoord,
        bool HasTexCoordF32 = false
    >
    inline void add()
    {
        if constexpr (HasTexCoord && !HasTexCoordF32)
        {
            add<HasColor, HasNormal, HasTexCoord, true>();
        }

        constexpr uint16_t Flags =
            (HasColor       ? VERTEX_COLOR    : 0) |
            (HasNormal      ? VERTEX_NORMAL   : 0) |
            (HasTexCoord    ? VERTEX_TEXCOORD : 0) |
            (HasTexCoordF32 ? TEXCOORD_F32    : 0) ;

        add(Flags);
    }

    void add(uint32_t attribs, uint32_t skips)
    {
        ASSERT(attribs == (attribs & (VERTEX_ATTRIB_MASK | TEXCOORD_F32)));
        ASSERT(skips == (skips & VERTEX_ATTRIB_MASK));
        ASSERT(skips != attribs || attribs == 0);
        ASSERT(skips == (skips & attribs));

        {
            MutexScope lock(m_mutex);

            bgfx::VertexLayout layout;
            layout.begin();

            if (attribs == VERTEX_POSITION)
            {
                layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
            }

            if (!!(skips & VERTEX_COLOR))
            {
                layout.skip(4 * sizeof(uint8_t));
            }
            else if (!!(attribs & VERTEX_COLOR))
            {
                layout.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
            }

            if (!!(skips & VERTEX_NORMAL))
            {
                layout.skip(4 * sizeof(uint8_t));
            }
            else if (!!(attribs & VERTEX_NORMAL))
            {
                layout.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true);
            }

            if (!!(skips & VERTEX_TEXCOORD))
            {
                layout.skip(2 * (!!(attribs & TEXCOORD_F32) ? sizeof(float) : sizeof(int16_t)));
            }
            else if (!!(attribs & VERTEX_TEXCOORD))
            {
                if (!!(attribs & TEXCOORD_F32))
                {
                    layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
                }
                else
                {
                    layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true);
                }
            }

            layout.end();
            ASSERT(layout.getStride() % 4 == 0);

            const uint16_t idx = get_index_from_flags(attribs, skips);

            ASSERT(m_layouts[idx].getStride() == 0);
            ASSERT(!bgfx::isValid(m_handles[idx]));

            m_layouts[idx] = layout;
            m_handles[idx] = bgfx::createVertexLayout(layout);
        } // MutexScope

        // Add variants with skipped attributes (for aliasing).
        if (attribs && !skips)
        {
            for (skips = VERTEX_COLOR; skips < (attribs & VERTEX_ATTRIB_MASK); skips++)
            {
                if ((attribs & VERTEX_ATTRIB_MASK) != (skips & VERTEX_ATTRIB_MASK) && (attribs & skips) == skips)
                {
                    add(attribs, skips);
                }
            }
        }
    }

private:
    Mutex                                m_mutex;
    Array<bgfx::VertexLayout, 128>       m_layouts;
    Array<bgfx::VertexLayoutHandle, 128> m_handles;
};


// -----------------------------------------------------------------------------
// VERTEX ATTRIB STATE
// -----------------------------------------------------------------------------

BX_ALIGN_DECL_16(struct) VertexAttribState
{
    uint8_t data[32];

    using PackedColorType    = uint32_t; // As RGBA_u8.

    using PackedNormalType   = uint32_t; // As RGB_u8.

    using PackedTexcoordType = uint32_t; // As RG_s16.

    using FullTexcoordType   = Vec2;

    template <typename ReturnT, size_t BytesOffset>
    const ReturnT* at() const
    {
        static_assert(is_pod<ReturnT>(),
            "ReturnT must be POD type.");

        static_assert(BytesOffset % std::alignment_of<ReturnT>::value == 0,
            "BytesOffset must be multiple of alignment of ReturnT.");

        return reinterpret_cast<const ReturnT*>(data + BytesOffset);
    }

    template <typename ReturnT, size_t BytesOffset>
    ReturnT* at()
    {
        return const_cast<ReturnT*>(static_cast<const VertexAttribState&>(*this).at<ReturnT, BytesOffset>());
    }
};

template <uint16_t Flags>
static constexpr size_t vertex_attribs_size()
{
    size_t size = 0;

    if constexpr (!!(Flags & VERTEX_COLOR))
    {
        size += sizeof(VertexAttribState::PackedColorType);
    }

    if constexpr (!!(Flags & VERTEX_NORMAL))
    {
        size += sizeof(VertexAttribState::PackedNormalType);
    }

    if constexpr (!!(Flags & VERTEX_TEXCOORD))
    {
        if constexpr (!!(Flags & TEXCOORD_F32))
        {
            size += sizeof(VertexAttribState::FullTexcoordType);
        }
        else
        {
            size += sizeof(VertexAttribState::PackedTexcoordType);
        }
    }

    return size;
}

template <uint16_t Flags, uint16_t Attrib>
static constexpr size_t vertex_attrib_offset()
{
    static_assert(
        Attrib ==  VERTEX_COLOR    ||
        Attrib ==  VERTEX_NORMAL   ||
        Attrib ==  VERTEX_TEXCOORD ||
        Attrib == (VERTEX_TEXCOORD | TEXCOORD_F32),
        "Invalid Attrib."
    );

    static_assert(
        Flags & Attrib,
        "Attrib must be part of Flags."
    );

    size_t offset = 0;

    // Order: color, normal, texcooord.

    if constexpr (Attrib != VERTEX_COLOR && (Flags & VERTEX_COLOR))
    {
        offset += sizeof(VertexAttribState::PackedColorType);
    }

    if constexpr (Attrib != VERTEX_NORMAL && (Flags & VERTEX_NORMAL))
    {
        offset += sizeof(VertexAttribState::PackedNormalType);
    }

    return offset;
}

struct VertexAttribStateFuncSet
{
    void (* color)(VertexAttribState&, uint32_t rgba) = nullptr;

    void (* normal)(VertexAttribState&, float nx, float ny, float nz) = nullptr;

    void (* texcoord)(VertexAttribState&, float u, float v) = nullptr;
};

class VertexAttribStateFuncTable
{
public:
    VertexAttribStateFuncTable()
    {
        //  +-------------------------- VERTEX_COLOR
        //  |  +----------------------- VERTEX_NORMAL
        //  |  |  +-------------------- VERTEX_TEXCOORD
        //  |  |  |
        add<0, 0, 0>();
        add<1, 0, 0>();
        add<0, 1, 0>();
        add<0, 0, 1>();
        add<1, 1, 0>();
        add<1, 0, 1>();
        add<0, 1, 1>();
        add<1, 1, 1>();
    }

    inline const VertexAttribStateFuncSet& operator[](uint16_t flags) const
    {
        return m_func_sets[get_index_from_flags(flags)];
    }

private:
    static inline constexpr uint16_t get_index_from_flags(uint16_t flags)
    {
        static_assert(
            VERTEX_ATTRIB_MASK >> VERTEX_ATTRIB_SHIFT == 0b0111 &&
            TEXCOORD_F32       >> 9                   == 0b1000,
            "Invalid index assumptions in `VertexAttribStateFuncTable::get_index_from_attribs`."
        );

        return
            ((flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
            ((flags & TEXCOORD_F32      ) >> 9                  ) ; // Bit 3.
    }

    template <uint16_t Flags>
    static void color(VertexAttribState& state, uint32_t rgba)
    {
        if constexpr (!!(Flags & VERTEX_COLOR))
        {
            *state.at<VertexAttribState::PackedColorType, vertex_attrib_offset<Flags, VERTEX_COLOR>()>() = bx::endianSwap(rgba);
        }
    }

    template <uint16_t Flags>
    static void normal(VertexAttribState& state, float nx, float ny, float nz)
    {
        if constexpr (!!(Flags & VERTEX_NORMAL))
        {
            const float normalized[] =
            {
                nx * 0.5f + 0.5f,
                ny * 0.5f + 0.5f,
                nz * 0.5f + 0.5f,
            };

            bx::packRgb8(state.at<VertexAttribState::PackedNormalType, vertex_attrib_offset<Flags, VERTEX_NORMAL>()>(), normalized);
        }
    }

    template <uint16_t Flags>
    static void texcoord(VertexAttribState& state, float u, float v)
    {
        if constexpr (!!(Flags & VERTEX_TEXCOORD))
        {
            if constexpr (!!(Flags & TEXCOORD_F32))
            {
                *state.at<VertexAttribState::FullTexcoordType, vertex_attrib_offset<Flags, VERTEX_TEXCOORD | TEXCOORD_F32>()>() = HMM_Vec2(u, v);
            }
            else
            {
                const float elems[] = { u, v };
                bx::packRg16S(state.at<VertexAttribState::PackedTexcoordType, vertex_attrib_offset<Flags, VERTEX_TEXCOORD>()>(), elems);
            }
        }
    }

    template <
        bool HasColor,
        bool HasNormal,
        bool HasTexCoord,
        bool HasTexCoordF32 = false
    >
    void add()
    {
        if constexpr (HasTexCoord && !HasTexCoordF32)
        {
            add<HasColor, HasNormal, HasTexCoord, true>();
        }

        constexpr uint16_t Flags =
            (HasColor       ? VERTEX_COLOR    : 0) |
            (HasNormal      ? VERTEX_NORMAL   : 0) |
            (HasTexCoord    ? VERTEX_TEXCOORD : 0) |
            (HasTexCoordF32 ? TEXCOORD_F32    : 0) ;

        VertexAttribStateFuncSet func_set;

        func_set.color    = color   <Flags>;
        func_set.normal   = normal  <Flags>;
        func_set.texcoord = texcoord<Flags>;

        constexpr uint16_t idx = get_index_from_flags(Flags);

        ASSERT(m_func_sets[idx].color == nullptr);

        m_func_sets[idx] = func_set;
    }

private:
    Array<VertexAttribStateFuncSet, 16> m_func_sets;
};


// -----------------------------------------------------------------------------
// GEOMETRY RECORDING
// -----------------------------------------------------------------------------

class MeshRecorder
{
public:
    void begin(uint16_t id, uint32_t flags, uint32_t extra_data = 0)
    {
        ASSERT(!is_recording() || (id == UINT16_MAX && flags == UINT32_MAX));

        m_id         = id;
        m_flags      = flags;
        m_extra_data = extra_data;

        m_position_buffer.clear();
        m_attrib_buffer  .clear();

        m_attrib_funcs     = flags != UINT32_MAX ? &ms_attrib_state_func_table[flags] : nullptr;
        m_vertex_func      = flags != UINT32_MAX ?  ms_vertex_push_func_table [flags] : nullptr;
        m_vertex_count     = 0;
        m_invocation_count = 0;
    }

    void end()
    {
        ASSERT(is_recording());

        begin(UINT16_MAX, UINT32_MAX);
    }

    inline void vertex(const Vec3& position)
    {
        ASSERT(is_recording());

        (* m_vertex_func)(*this, position);
    }

    inline void color(uint32_t rgba)
    {
        ASSERT(is_recording());

        m_attrib_funcs->color(m_attrib_state, rgba);
    }

    inline void normal(float nx, float ny, float nz)
    {
        ASSERT(is_recording());

        m_attrib_funcs->normal(m_attrib_state, nx, ny, nz);
    }

    inline void texcoord(float u, float v)
    {
        ASSERT(is_recording());

        m_attrib_funcs->texcoord(m_attrib_state, u, v);
    }

    inline const Vector<uint8_t>& attrib_buffer() const
    {
        ASSERT(is_recording());

        return m_attrib_buffer;
    }

    inline const Vector<uint8_t>& position_buffer() const
    {
        ASSERT(is_recording());

        return m_position_buffer;
    }

    inline bool is_recording() const { return m_id != UINT16_MAX; }

    inline uint16_t id() const { return m_id; }

    inline uint32_t flags() const { return m_flags; }

    inline uint32_t extra_data() const { return m_extra_data; }

    inline uint32_t vertex_count() const { return m_vertex_count; }

private:
    using VertexPushFunc = void (*)(MeshRecorder&, const Vec3&);

    class VertexPushFuncTable
    {
    public:
        VertexPushFuncTable()
        {
            m_funcs.fill(nullptr);

            //  +-------------------------- VERTEX_COLOR
            //  |  +----------------------- VERTEX_NORMAL
            //  |  |  +-------------------- VERTEX_TEXCOORD
            //  |  |  |
            add<0, 0, 0>();
            add<1, 0, 0>();
            add<0, 1, 0>();
            add<0, 0, 1>();
            add<1, 1, 0>();
            add<1, 0, 1>();
            add<0, 1, 1>();
            add<1, 1, 1>();
        }

        inline const VertexPushFunc& operator[](uint32_t flags) const
        {
            return m_funcs[get_index_from_flags(static_cast<uint16_t>(flags))];
        }

    private:
        static inline constexpr uint16_t get_index_from_flags(uint16_t flags)
        {
            static_assert(
                VERTEX_ATTRIB_MASK >> VERTEX_ATTRIB_SHIFT == 0b00111 &&
                TEXCOORD_F32       >> 9                   == 0b01000 &&
                PRIMITIVE_QUADS                           == 0b10000,
                "Invalid index assumptions in `MeshRecorder::VertexPushFuncTable::get_index_from_attribs`."
            );

            return
                ((flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
                ((flags & TEXCOORD_F32      ) >> 9                  ) | // Bit 3.
                ((flags & PRIMITIVE_QUADS   )                       ) ; // Bit 4.
        }

        template <size_t Size>
        static inline void emulate_quad(Vector<uint8_t>& buffer)
        {
            static_assert(Size > 0, "Size must be positive.");

            ASSERT(!buffer.empty());
            ASSERT( buffer.size() % Size      == 0);
            ASSERT((buffer.size() / Size) % 3 == 0);

            buffer.resize(buffer.size() + 2 * Size);

            uint8_t* end = buffer.data() + buffer.size();

            // Assuming the last triangle has relative indices
            // [v0, v1, v2] = [-5, -4, -3], we need to copy the vertices v0 and v2.
            assign<Size>(end - 5 * Size, end - 2 * Size);
            assign<Size>(end - 3 * Size, end - 1 * Size);
        }

        template <uint16_t Flags>
        static void vertex(MeshRecorder& mesh_recorder, const Vec3& position)
        {
            if constexpr (!!(Flags & (PRIMITIVE_QUADS)))
            {
                if ((mesh_recorder.m_invocation_count & 3) == 3)
                {
                    emulate_quad<sizeof(position)>(mesh_recorder.m_position_buffer);

                    if constexpr (!!(Flags & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)))
                    {
                        emulate_quad<vertex_attribs_size<Flags>()>(mesh_recorder.m_attrib_buffer);
                    }

                    mesh_recorder.m_vertex_count += 2;
                }

                mesh_recorder.m_invocation_count++;
            }

            mesh_recorder.m_vertex_count++;

            push_back(mesh_recorder.m_position_buffer, position);

            if constexpr (!!(Flags & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)))
            {
                push_back<vertex_attribs_size<Flags>()>(mesh_recorder.m_attrib_buffer, mesh_recorder.m_attrib_state.data);
            }
        }

        template <
            bool HasColor,
            bool HasNormal,
            bool HasTexCoord,
            bool HasTexCoordF32    = false,
            bool HasPrimitiveQuads = false
        >
        void add()
        {
            if constexpr (HasTexCoord && !HasTexCoordF32)
            {
                add<HasColor, HasNormal, HasTexCoord, true, HasPrimitiveQuads>();
            }

            if constexpr (!HasPrimitiveQuads)
            {
                add<HasColor, HasNormal, HasTexCoord, HasTexCoordF32, true>();
            }

            constexpr uint16_t Flags =
                (HasColor          ? VERTEX_COLOR    : 0) |
                (HasNormal         ? VERTEX_NORMAL   : 0) |
                (HasTexCoord       ? VERTEX_TEXCOORD : 0) |
                (HasTexCoordF32    ? TEXCOORD_F32    : 0) |
                (HasPrimitiveQuads ? PRIMITIVE_QUADS : 0) ;

            // NOTE : We do insert few elements multiple times.
            m_funcs[get_index_from_flags(Flags)] = vertex<Flags>;
        }

    private:
        Array<VertexPushFunc, 32> m_funcs;
    };

protected:
    Vector<uint8_t>                         m_attrib_buffer;
    Vector<uint8_t>                         m_position_buffer;
    VertexAttribState                       m_attrib_state;
    const VertexAttribStateFuncSet*         m_attrib_funcs     = nullptr;
    VertexPushFunc                          m_vertex_func      = nullptr;
    uint32_t                                m_vertex_count     = 0;
    uint32_t                                m_invocation_count = 0;
    uint32_t                                m_extra_data       = 0;
    uint32_t                                m_flags            = UINT32_MAX;
    uint16_t                                m_id               = UINT16_MAX;

    static const VertexAttribStateFuncTable ms_attrib_state_func_table;
    static const VertexPushFuncTable        ms_vertex_push_func_table;
};

const VertexAttribStateFuncTable MeshRecorder::ms_attrib_state_func_table;

const MeshRecorder::VertexPushFuncTable MeshRecorder::ms_vertex_push_func_table;


// -----------------------------------------------------------------------------
// INSTANCE RECORDING
// -----------------------------------------------------------------------------

class InstanceRecorder
{
public:
    void begin(uint16_t id, uint16_t type)
    {
        ASSERT(!is_recording() || (id == UINT16_MAX && type == UINT16_MAX));

        constexpr uint16_t type_sizes[] =
        {
            sizeof(Mat4), // INSTANCE_TRANSFORM
            16,           // INSTANCE_DATA_16
            32,           // INSTANCE_DATA_32
            48,           // INSTANCE_DATA_48
            64,           // INSTANCE_DATA_64
            80,           // INSTANCE_DATA_80
            96,           // INSTANCE_DATA_96
            112,          // INSTANCE_DATA_112
        };

        m_id            = id;
        m_instance_size = type_sizes[std::max<size_t>(type, BX_COUNTOF(type_sizes) - 1)];
        m_is_transform  = type == INSTANCE_TRANSFORM;

        m_buffer.clear();
    }

    inline void end()
    {
        ASSERT(is_recording());

        begin(UINT16_MAX, UINT16_MAX);
    }

    inline void instance(const void* data)
    {
        ASSERT(data);
        ASSERT(is_recording());

        push_back(m_buffer, data, m_instance_size);
    }

    inline const Vector<uint8_t>& buffer() const
    {
        ASSERT(is_recording());

        return m_buffer;
    }

    inline uint32_t instance_count() const
    {
        return static_cast<uint32_t>(m_buffer.size() / m_instance_size);
    }

    inline bool is_recording() const { return m_id != UINT16_MAX; }

    inline uint16_t id() const { return m_id; }

    inline uint16_t instance_size() const { return m_instance_size; }

    inline bool is_transform() const { return m_is_transform; }

private:
    Vector<uint8_t> m_buffer;
    uint16_t        m_id            = UINT16_MAX;
    uint16_t        m_instance_size = 0;
    bool            m_is_transform  = false;
};


// -----------------------------------------------------------------------------
// MESH
// -----------------------------------------------------------------------------

union VertexBufferUnion
{
    uint16_t                        transient_index = bgfx::kInvalidHandle;
    bgfx::VertexBufferHandle        static_buffer;
    bgfx::DynamicVertexBufferHandle dynamic_buffer;
};

union IndexBufferUnion
{
    uint16_t                       transient_index = bgfx::kInvalidHandle;
    bgfx::IndexBufferHandle        static_buffer;
    bgfx::DynamicIndexBufferHandle dynamic_buffer;
};

struct Mesh
{
    uint32_t          element_count = 0;
    uint32_t          extra_data    = 0;
    uint32_t          flags         = MESH_INVALID;
    VertexBufferUnion positions;
    VertexBufferUnion attribs;
    IndexBufferUnion  indices;
    uint8_t           _pad[2];

    inline uint16_t type() const
    {
        return mesh_type(flags);
    }

    void destroy()
    {
        switch (type())
        {
        case MESH_STATIC:
            bgfx::destroy   (positions.static_buffer);
            destroy_if_valid(attribs  .static_buffer);
            bgfx::destroy   (indices  .static_buffer);
            break;

        case MESH_DYNAMIC:
            bgfx::destroy   (positions.dynamic_buffer);
            destroy_if_valid(attribs  .dynamic_buffer);
            bgfx::destroy   (indices  .dynamic_buffer);
            break;

        default:
            break;
        }

        *this = {};
    }
};


// -----------------------------------------------------------------------------
// MESH CACHE
// -----------------------------------------------------------------------------

struct MeshCache
{
public:
    bool add_mesh(const MeshRecorder& recorder, const VertexLayoutCache& layouts)
    {
        ASSERT(recorder.id() < m_meshes.size());

        MutexScope lock(m_mutex);

        Mesh& mesh = m_meshes[recorder.id()];

        const uint16_t new_type = mesh_type(recorder.flags());

        if (new_type == MESH_INVALID)
        {
            ASSERT(false && "Invalid registered mesh type.");
            return false;
        }

        mesh.destroy();

        mesh.element_count = recorder.vertex_count();
        mesh.extra_data    = recorder.extra_data();
        mesh.flags         = recorder.flags();

        switch (new_type)
        {
        case MESH_STATIC:
        case MESH_DYNAMIC:
            add_persistent_mesh(mesh, recorder, layouts);
            break;

        case MESH_TRANSIENT:
            if (add_transient_mesh(mesh, recorder, layouts))
            {
                m_transient_idxs.push_back(recorder.id());
            }
            break;

        default:
            break;
        }

        return true;
    }

    void clear()
    {
        MutexScope lock(m_mutex);

        for (Mesh& mesh : m_meshes)
        {
            mesh.destroy();
        }
    }

    void clear_transient_meshes()
    {
        MutexScope lock(m_mutex);

        for (uint16_t idx : m_transient_idxs)
        {
            ASSERT(m_meshes[idx].type() == MESH_TRANSIENT);

            m_meshes[idx] = {};
        }

        m_transient_idxs   .clear();
        m_transient_buffers.clear();

        m_transient_exhausted = false;
    }

    inline Mesh& operator[](uint16_t id) { return m_meshes[id]; }

    inline const Mesh& operator[](uint16_t id) const { return m_meshes[id]; }

    inline const Vector<bgfx::TransientVertexBuffer>& transient_buffers() const
    {
        return m_transient_buffers;
    }

private:
    bool add_transient_buffer(const Vector<uint8_t>& data, const bgfx::VertexLayout& layout, uint16_t& dst_index)
    {
        ASSERT(layout.getStride() > 0);

        if (data.empty())
        {
            return true;
        }

        if (data.size() % layout.getStride() != 0)
        {
            ASSERT(false && "Layout does not match data size.");
            return false;
        }

        const uint32_t count = static_cast<uint32_t>(data.size() / layout.getStride());

        if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
        {
            // No assert here as it can happen and we'll just skip that geometry.
            return false;
        }

        ASSERT(m_transient_buffers.size() < UINT16_MAX);

        dst_index = static_cast<uint16_t>(m_transient_buffers.size());
        m_transient_buffers.resize(m_transient_buffers.size() + 1);

        bgfx::allocTransientVertexBuffer(&m_transient_buffers.back(), count, layout);
        (void)memcpy(m_transient_buffers.back().data, data.data(), data.size());

        return true;
    }

    bool add_transient_mesh(Mesh& mesh, const MeshRecorder& recorder, const VertexLayoutCache& layouts)
    {
        ASSERT(!recorder.position_buffer().empty());

        if (!m_transient_exhausted)
        {
            if (!add_transient_buffer(recorder.position_buffer(), layouts[VERTEX_POSITION], mesh.positions.transient_index) ||
                !add_transient_buffer(recorder.attrib_buffer  (), layouts[mesh.flags     ], mesh.attribs  .transient_index)
            )
            {
                m_transient_exhausted = true;
                mesh = {};
            }
        }

        return !m_transient_exhausted;
    }

    void add_persistent_mesh(Mesh& mesh, const MeshRecorder& recorder, const VertexLayoutCache& layout_cache)
    {
        ASSERT(mesh.type() == MESH_STATIC || mesh.type() == MESH_DYNAMIC);

        meshopt_Stream            streams[2];
        const bgfx::VertexLayout* layouts[2];

        layouts[0] = &layout_cache[VERTEX_POSITION]; // TODO : Eventually add support for 2D position.
        streams[0] = { recorder.position_buffer().data(), layouts[0]->getStride(), layouts[0]->getStride() };

        const bool has_attribs = (mesh_attribs(mesh.flags) & VERTEX_ATTRIB_MASK);

        if (has_attribs)
        {
            layouts[1] = &layout_cache[mesh.flags];
            streams[1] = { recorder.attrib_buffer().data(), layouts[1]->getStride(), layouts[1]->getStride() };
        }

        Vector<unsigned int> remap_table(mesh.element_count);
        uint32_t             indexed_vertex_count = 0;

        if (has_attribs)
        {
            indexed_vertex_count = static_cast<uint32_t>(meshopt_generateVertexRemapMulti(
                remap_table.data(), nullptr, mesh.element_count, mesh.element_count, streams, BX_COUNTOF(streams)
            ));

            if (mesh.type() == MESH_STATIC)
            {
                update_persistent_vertex_buffer(
                    streams[1], *layouts[1], mesh.element_count, indexed_vertex_count, remap_table, mesh.attribs.static_buffer
                );
            }
            else
            {
                update_persistent_vertex_buffer(
                    streams[1], *layouts[1], mesh.element_count, indexed_vertex_count, remap_table, mesh.attribs.dynamic_buffer
                );
            }
        }
        else
        {
            indexed_vertex_count = static_cast<uint32_t>(meshopt_generateVertexRemap(
                remap_table.data(), nullptr, mesh.element_count, streams[0].data, mesh.element_count, streams[0].size
            ));
        }

        void* vertex_positions = nullptr;
        if (mesh.type() == MESH_STATIC)
        {
            update_persistent_vertex_buffer(
                streams[0], *layouts[0], mesh.element_count, indexed_vertex_count, remap_table, mesh.positions.static_buffer, &vertex_positions
            );
        }
        else
        {
            update_persistent_vertex_buffer(
                streams[0], *layouts[0], mesh.element_count, indexed_vertex_count, remap_table, mesh.positions.dynamic_buffer, &vertex_positions
            );
        }

        const bool optimize_geometry = (mesh.flags & OPTIMIZE_GEOMETRY) && ((mesh.flags & PRIMITIVE_TYPE_MASK) <= PRIMITIVE_QUADS);

        if (mesh.type() == MESH_STATIC)
        {
            update_persistent_index_buffer(
                mesh.element_count,
                indexed_vertex_count,
                remap_table,
                optimize_geometry,
                static_cast<float*>(vertex_positions),
                mesh.indices.static_buffer
            );
        }
        else
        {
            update_persistent_index_buffer(
                mesh.element_count,
                indexed_vertex_count,
                remap_table,
                optimize_geometry,
                static_cast<float*>(vertex_positions),
                mesh.indices.dynamic_buffer
            );
        }
    }

    template <typename BufferT>
    inline static void update_persistent_vertex_buffer
    (
        const meshopt_Stream&       stream,
        const bgfx::VertexLayout&   layout,
        uint32_t                    vertex_count,
        uint32_t                    indexed_vertex_count,
        const Vector<unsigned int>& remap_table,
        BufferT&                    dst_buffer_handle,
        void**                      dst_remapped_memory = nullptr
    )
    {
        static_assert(
            std::is_same<BufferT, bgfx::       VertexBufferHandle>::value ||
            std::is_same<BufferT, bgfx::DynamicVertexBufferHandle>::value,
            "Unsupported vertex buffer type for update."
        );

        const bgfx::Memory* memory = bgfx::alloc(static_cast<uint32_t>(indexed_vertex_count * stream.size));
        ASSERT(memory && memory->data);

        meshopt_remapVertexBuffer(memory->data, stream.data, vertex_count, stream.size, remap_table.data());

        if (dst_remapped_memory)
        {
            *dst_remapped_memory = memory->data;
        }

        if constexpr (std::is_same<BufferT, bgfx::VertexBufferHandle>::value)
        {
            dst_buffer_handle = bgfx::createVertexBuffer(memory, layout);
        }

        if constexpr (std::is_same<BufferT, bgfx::DynamicVertexBufferHandle>::value)
        {
            dst_buffer_handle = bgfx::createDynamicVertexBuffer(memory, layout);
        }

        ASSERT(bgfx::isValid(dst_buffer_handle));
    }

    template <typename T>
    inline static void remap_index_buffer
    (
        uint32_t                    vertex_count,
        uint32_t                    indexed_vertex_count,
        const Vector<unsigned int>& remap_table,
        bool                        optimize,
        const float*                vertex_positions,
        T*                          dst_indices
    )
    {
        meshopt_remapIndexBuffer<T>(dst_indices, nullptr, vertex_count, remap_table.data());

        if (optimize && vertex_positions)
        {
            meshopt_optimizeVertexCache<T>(dst_indices, dst_indices, vertex_count, indexed_vertex_count);

            meshopt_optimizeOverdraw(dst_indices, dst_indices, vertex_count, vertex_positions, indexed_vertex_count, 3 * sizeof(float), 1.05f);

            // meshopt_optimizeVertexFetch(vertices, dst_indices, vertex_count, vertex_positions, indexed_vertex_count, 3 * sizeof(float));
        }
    }

    template <typename BufferT>
    inline static void update_persistent_index_buffer
    (
        uint32_t                    vertex_count,
        uint32_t                    indexed_vertex_count,
        const Vector<unsigned int>& remap_table,
        bool                        optimize,
        const float*                vertex_positions,
        BufferT&                    dst_buffer_handle
    )
    {
        static_assert(
            std::is_same<BufferT, bgfx::       IndexBufferHandle>::value ||
            std::is_same<BufferT, bgfx::DynamicIndexBufferHandle>::value,
            "Unsupported index buffer type for update."
        );

        uint16_t buffer_flags = BGFX_BUFFER_NONE;
        uint32_t type_size    = sizeof(uint16_t);

        if (indexed_vertex_count > UINT16_MAX)
        {
            buffer_flags = BGFX_BUFFER_INDEX32;
            type_size    = sizeof(uint32_t);
        }

        const bgfx::Memory* memory = bgfx::alloc(vertex_count * type_size);
        ASSERT(memory && memory->data);

        type_size == sizeof(uint16_t)
            ? remap_index_buffer(vertex_count, indexed_vertex_count, remap_table, optimize, vertex_positions, reinterpret_cast<uint16_t*>(memory->data))
            : remap_index_buffer(vertex_count, indexed_vertex_count, remap_table, optimize, vertex_positions, reinterpret_cast<uint32_t*>(memory->data));

        if constexpr (std::is_same<BufferT, bgfx::IndexBufferHandle>::value)
        {
            dst_buffer_handle = bgfx::createIndexBuffer(memory, buffer_flags);
        }

        if constexpr (std::is_same<BufferT, bgfx::DynamicIndexBufferHandle>::value)
        {
            dst_buffer_handle = bgfx::createDynamicIndexBuffer(memory, buffer_flags);
        }

        ASSERT(bgfx::isValid(dst_buffer_handle));
    }

private:
    Mutex                               m_mutex;
    Array<Mesh, MAX_MESHES>             m_meshes;
    Vector<uint16_t>                    m_transient_idxs;
    Vector<bgfx::TransientVertexBuffer> m_transient_buffers;
    bool                                m_transient_exhausted = false;
};


// -----------------------------------------------------------------------------
// INSTANCE CACHE
// -----------------------------------------------------------------------------

struct InstanceData
{
    bgfx::InstanceDataBuffer buffer       = { nullptr, 0, 0, 0, 0, BGFX_INVALID_HANDLE };
    bool                     is_transform = false;
    uint8_t                  _pad[7];
};

class InstanceCache
{
public:
    bool add_buffer(const InstanceRecorder& recorder)
    {
        ASSERT(recorder.id() < m_data.size());

        MutexScope lock(m_mutex);

        const uint32_t count     = recorder.instance_count();
        const uint16_t stride    = recorder.instance_size ();
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(count, stride);

        if (available < count)
        {
            // TODO : Handle this, inform user.
            return false;
        }

        InstanceData &data = m_data[recorder.id()];
        data.is_transform = recorder.is_transform();
        bgfx::allocInstanceDataBuffer(&data.buffer, count, stride);
        (void)memcpy(data.buffer.data, recorder.buffer().data(), recorder.buffer().size());

        return true;
    }

    inline const InstanceData& operator[](uint16_t id) const
    {
        return m_data[id];
    }

private:
    Mutex                                     m_mutex;
    Array<InstanceData, MAX_INSTANCE_BUFFERS> m_data;
};


// -----------------------------------------------------------------------------
// GEOMETRY SUBMISSION
// -----------------------------------------------------------------------------

static inline uint64_t translate_draw_state_flags(uint16_t flags)
{
    if (flags == STATE_DEFAULT)
    {
        static_assert(
            STATE_DEFAULT      == (STATE_WRITE_RGB            |
                                   STATE_WRITE_A              |
                                   STATE_WRITE_Z              |
                                   STATE_DEPTH_TEST_LESS      |
                                   STATE_CULL_CW              |
                                   STATE_MSAA                 ) &&

            BGFX_STATE_DEFAULT == (BGFX_STATE_WRITE_RGB       |
                                   BGFX_STATE_WRITE_A         |
                                   BGFX_STATE_WRITE_Z         |
                                   BGFX_STATE_DEPTH_TEST_LESS |
                                   BGFX_STATE_CULL_CW         |
                                   BGFX_STATE_MSAA            ),

            "BGFX and MiNiMo default draw states don't match."
        );

        return BGFX_STATE_DEFAULT;
    }

    constexpr uint32_t BLEND_STATE_MASK       = STATE_BLEND_ADD | STATE_BLEND_ALPHA | STATE_BLEND_MAX | STATE_BLEND_MIN;
    constexpr uint32_t BLEND_STATE_SHIFT      = 0;

    constexpr uint32_t CULL_STATE_MASK        = STATE_CULL_CCW | STATE_CULL_CW;
    constexpr uint32_t CULL_STATE_SHIFT       = 4;

    constexpr uint32_t DEPTH_TEST_STATE_MASK  = STATE_DEPTH_TEST_GEQUAL | STATE_DEPTH_TEST_GREATER | STATE_DEPTH_TEST_LEQUAL | STATE_DEPTH_TEST_LESS;
    constexpr uint32_t DEPTH_TEST_STATE_SHIFT = 6;

    constexpr uint64_t blend_table[] =
    {
        0,
        BGFX_STATE_BLEND_ADD,
        BGFX_STATE_BLEND_ALPHA,
        BGFX_STATE_BLEND_LIGHTEN,
        BGFX_STATE_BLEND_DARKEN,
    };

    constexpr uint64_t cull_table[] =
    {
        0,
        BGFX_STATE_CULL_CCW,
        BGFX_STATE_CULL_CW,
    };

    constexpr uint64_t depth_test_table[] =
    {
        0,
        BGFX_STATE_DEPTH_TEST_GEQUAL,
        BGFX_STATE_DEPTH_TEST_GREATER,
        BGFX_STATE_DEPTH_TEST_LEQUAL,
        BGFX_STATE_DEPTH_TEST_LESS,
    };

    return
        blend_table     [(flags & BLEND_STATE_MASK     ) >> BLEND_STATE_SHIFT         ] |
        cull_table      [(flags & CULL_STATE_MASK      ) >> CULL_STATE_SHIFT          ] |
        depth_test_table[(flags & DEPTH_TEST_STATE_MASK) >> DEPTH_TEST_STATE_SHIFT    ] |
        (                (flags & STATE_MSAA           ) ?  BGFX_STATE_MSAA        : 0) |
        (                (flags & STATE_WRITE_A        ) ?  BGFX_STATE_WRITE_A     : 0) |
        (                (flags & STATE_WRITE_RGB      ) ?  BGFX_STATE_WRITE_RGB   : 0) |
        (                (flags & STATE_WRITE_Z        ) ?  BGFX_STATE_WRITE_Z     : 0);
}

static void submit_mesh
(
    const Mesh&                                mesh,
    const Mat4&                                transform,
    const DrawState&                           state,
    const Vector<bgfx::TransientVertexBuffer>& transient_buffers,
    const DefaultUniforms&                     default_uniforms,
    bgfx::Encoder&                             encoder
)
{
    static const uint64_t primitive_flags[] =
    {
        0, // Triangles.
        0, // Quads (for users, triangles internally).
        BGFX_STATE_PT_TRISTRIP,
        BGFX_STATE_PT_LINES,
        BGFX_STATE_PT_LINESTRIP,
        BGFX_STATE_PT_POINTS,
    };

    switch (mesh.type())
    {
    case MESH_TRANSIENT:
                                      encoder.setVertexBuffer(0, &transient_buffers[mesh.positions.transient_index], state.element_start, state.element_count);
        if (mesh_attribs(mesh.flags)) encoder.setVertexBuffer(1, &transient_buffers[mesh.attribs  .transient_index], state.element_start, state.element_count, state.vertex_alias);
        break;

    case MESH_STATIC:
                                      encoder.setVertexBuffer(0, mesh.positions.static_buffer);
        if (mesh_attribs(mesh.flags)) encoder.setVertexBuffer(1, mesh.attribs  .static_buffer, 0, UINT32_MAX, state.vertex_alias);
                                      encoder.setIndexBuffer (   mesh.indices  .static_buffer, state.element_start, state.element_count);
        break;

    case MESH_DYNAMIC:
                                      encoder.setVertexBuffer(0, mesh.positions.static_buffer);
        if (mesh_attribs(mesh.flags)) encoder.setVertexBuffer(1, mesh.attribs  .static_buffer, 0, UINT32_MAX, state.vertex_alias);
                                      encoder.setIndexBuffer (   mesh.indices  .static_buffer, state.element_start, state.element_count);
        break;

    default:
        ASSERT(false && "Invalid mesh type.");
        break;
    }

    if (bgfx::isValid(state.texture) && bgfx::isValid(state.sampler))
    {
        encoder.setTexture(0, state.sampler, state.texture);
    }

    if (mesh.flags & VERTEX_PIXCOORD)
    {
        const float data[] =
        {
            static_cast<float>(state.texture_size[0]),
            static_cast<float>(state.texture_size[1]),
            static_cast<float>(state.texture_size[0]) ? 1.0f / static_cast<float>(state.texture_size[0]) : 0.0f,
            static_cast<float>(state.texture_size[1]) ? 1.0f / static_cast<float>(state.texture_size[1]) : 0.0f
        };

        encoder.setUniform(default_uniforms.texture_size, data);
    }

    encoder.setTransform(&transform);

    uint64_t flags = translate_draw_state_flags(state.flags);

    flags |= primitive_flags[(mesh.flags & PRIMITIVE_TYPE_MASK) >> PRIMITIVE_TYPE_SHIFT];

    encoder.setState(flags);

    ASSERT(bgfx::isValid(state.program));
    encoder.submit(state.pass, state.program);
}


// -----------------------------------------------------------------------------
// TEXTURING
// -----------------------------------------------------------------------------

struct Texture
{
    bgfx::TextureFormat::Enum   format      = bgfx::TextureFormat::Count;
    bgfx::BackbufferRatio::Enum ratio       = bgfx::BackbufferRatio::Count;
    uint32_t                    read_frame  = UINT32_MAX;
    uint16_t                    width       = 0;
    uint16_t                    height      = 0;
    bgfx::TextureHandle         blit_handle = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle         handle      = BGFX_INVALID_HANDLE;

    void destroy()
    {
        ASSERT(bgfx::isValid(blit_handle) <= bgfx::isValid(handle));

        if (bgfx::isValid(blit_handle))
        {
            bgfx::destroy(blit_handle);
        }

        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
            *this = {};
        }
    }
};

class TextureCache
{
public:
    void clear()
    {
        MutexScope lock(m_mutex);

        for (Texture& texture : m_textures)
        {
            texture.destroy();
        }
    }

    void add_texture(uint16_t id, uint16_t flags, uint16_t width, uint16_t height, uint16_t stride, const void* data)
    {
        ASSERT(id < m_textures.size());

        MutexScope lock(m_mutex);

        Texture& texture = m_textures[id];
        texture.destroy();

        static const uint64_t sampling_flags[] =
        {
            BGFX_SAMPLER_NONE,
            BGFX_SAMPLER_POINT,
        };

        static const uint64_t border_flags[] =
        {
            BGFX_SAMPLER_NONE,
            BGFX_SAMPLER_UVW_MIRROR,
            BGFX_SAMPLER_UVW_CLAMP,
        };

        static const uint64_t target_flags[] =
        {
            BGFX_TEXTURE_NONE,
            BGFX_TEXTURE_RT,
        };

        static const struct Format
        {
            uint32_t                  size;
            bgfx::TextureFormat::Enum type;

        } formats[] =
        {
            { 4, bgfx::TextureFormat::RGBA8 },
            { 1, bgfx::TextureFormat::R8    },
            { 0, bgfx::TextureFormat::D24S8 },
            { 0, bgfx::TextureFormat::D32F  },
        };

        const Format format = formats[(flags & TEXTURE_FORMAT_MASK) >> TEXTURE_FORMAT_SHIFT];

        bgfx::BackbufferRatio::Enum ratio = bgfx::BackbufferRatio::Count;

        if (width >= SIZE_EQUAL && width <= SIZE_DOUBLE && width == height)
        {
            ratio = static_cast<bgfx::BackbufferRatio::Enum>(width - SIZE_EQUAL);
        }

        const bgfx::Memory* memory = nullptr;

        if (data && format.size > 0 && ratio == bgfx::BackbufferRatio::Count)
        {
            if (stride == 0 || stride == width * format.size)
            {
                memory = bgfx::copy(data, width * height * format.size);
            }
            else
            {
                const uint8_t* src = static_cast<const uint8_t*>(data);
                uint8_t*       dst = memory->data;

                for (uint16_t y = 0; y < height; y++)
                {
                    (void)memcpy(dst, src, width * format.size);

                    src += stride;
                    dst += width * format.size;
                }
            }
        }

        const uint64_t texture_flags =
            sampling_flags[(flags & TEXTURE_SAMPLING_MASK) >> TEXTURE_SAMPLING_SHIFT] |
            border_flags  [(flags & TEXTURE_BORDER_MASK  ) >> TEXTURE_BORDER_SHIFT  ] |
            target_flags  [(flags & TEXTURE_TARGET_MASK  ) >> TEXTURE_TARGET_SHIFT  ] ;

        if (ratio == bgfx::BackbufferRatio::Count)
        {
            texture.handle = bgfx::createTexture2D(width, height, false, 1, format.type, texture_flags, memory);
        }
        else
        {
            ASSERT(!memory);
            texture.handle = bgfx::createTexture2D(ratio, false, 1, format.type, texture_flags);
        }
        ASSERT(bgfx::isValid(texture.handle));

        texture.format = format.type;
        texture.ratio  = ratio;
        texture.width  = width;
        texture.height = height;
    }

    void destroy_texture(uint16_t id)
    {
        ASSERT(id < m_textures.size());

        MutexScope lock(m_mutex);

        m_textures[id].destroy();
    }

    void schedule_read(uint16_t id, bgfx::ViewId pass, bgfx::Encoder* encoder, void* data)
    {
        MutexScope lock(m_mutex);

        Texture& texture = m_textures[id];
        ASSERT(bgfx::isValid(texture.handle));

        if (!bgfx::isValid(texture.blit_handle))
        {
            constexpr uint64_t flags =
                BGFX_TEXTURE_BLIT_DST  |
                BGFX_TEXTURE_READ_BACK |
                BGFX_SAMPLER_MIN_POINT |
                BGFX_SAMPLER_MAG_POINT |
                BGFX_SAMPLER_MIP_POINT |
                BGFX_SAMPLER_U_CLAMP   |
                BGFX_SAMPLER_V_CLAMP   ;

            texture.blit_handle = texture.ratio == bgfx::BackbufferRatio::Count
                ? texture.blit_handle = bgfx::createTexture2D(texture.width, texture.height, false, 1, texture.format, flags)
                : texture.blit_handle = bgfx::createTexture2D(texture.ratio                , false, 1, texture.format, flags);

            ASSERT(bgfx::isValid(texture.blit_handle));
        }

        encoder->blit(pass, texture.blit_handle, 0, 0, texture.handle);

        texture.read_frame = bgfx::readTexture(texture.blit_handle, data);
    }

    inline const Texture& operator[](uint16_t id) const { return m_textures[id]; }

private:
    Mutex                        m_mutex;
    Array<Texture, MAX_TEXTURES> m_textures;
};


// -----------------------------------------------------------------------------
// FRAMEBUFFERS
// -----------------------------------------------------------------------------

struct Framebuffer
{
    bgfx::FrameBufferHandle handle = BGFX_INVALID_HANDLE;
    uint16_t                width  = 0;
    uint16_t                height = 0;

    void destroy()
    {
        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
            *this = {};
        }
    }
};

class FramebufferRecorder
{
public:
    inline void begin(uint16_t id)
    {
        ASSERT(!is_recording() || id == UINT16_MAX);

        m_id     = id;
        m_width  = 0;
        m_height = 0;
        m_textures.clear();
    }

    inline void add_texture(const Texture& texture)
    {
        ASSERT(is_recording());

        if (m_textures.empty())
        {
            ASSERT(texture.width  > 0);
            ASSERT(texture.height > 0);

            m_width  = texture.width;
            m_height = texture.height;
        }

        m_textures.push_back(texture.handle);
    }

    inline void end()
    {
        ASSERT(is_recording());

        begin(UINT16_MAX);
    }

    Framebuffer create_framebuffer() const
    {
        ASSERT(is_recording());

        Framebuffer framebuffer;

        if (!m_textures.empty())
        {
            framebuffer.handle = bgfx::createFrameBuffer(static_cast<uint8_t>(m_textures.size()), m_textures.data(), false);
            ASSERT(bgfx::isValid(framebuffer.handle));

            framebuffer.width  = m_width;
            framebuffer.height = m_height;
        }

        return framebuffer;
    }

    inline bool is_recording() const { return m_id != UINT16_MAX; }

    inline uint16_t id() const { return m_id; }

private:
    Vector<bgfx::TextureHandle> m_textures;
    uint16_t                    m_id     = UINT16_MAX;
    uint16_t                    m_width  = 0;
    uint16_t                    m_height = 0;
};

class FramebufferCache
{
public:
    void clear()
    {
        MutexScope lock(m_mutex);

        for (Framebuffer& framebuffer : m_framebuffers)
        {
            framebuffer.destroy();
        }
    }

    void add_framebuffer(const FramebufferRecorder& mesh_recorder)
    {
        MutexScope lock(m_mutex);

        Framebuffer& framebuffer = m_framebuffers[mesh_recorder.id()];
        framebuffer.destroy();
        framebuffer = mesh_recorder.create_framebuffer();
    }

    inline const Framebuffer& operator[](uint16_t id) const { return m_framebuffers[id]; }

private:
    Mutex                                m_mutex;
    Array<Framebuffer, MAX_FRAMEBUFFERS> m_framebuffers;
};


// -----------------------------------------------------------------------------
// UTF-8 DECODING
//
// http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
// -----------------------------------------------------------------------------

enum
{
    UTF8_ACCEPT,
    UTF8_REJECT,
};

static constexpr uint8_t s_utf8_data[] =
{
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
    0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12,
};

static inline uint32_t utf8_decode(uint32_t* state, uint32_t* codepoint, uint32_t byte)
{
    uint32_t type = s_utf8_data[byte];

    *codepoint = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codepoint << 6) : (0xff >> type) & (byte);

    *state = s_utf8_data[256 + *state + type];

    return *state;
}


// -----------------------------------------------------------------------------
// ATLAS RECORDING / BUILDING / CACHING
// -----------------------------------------------------------------------------

class FontDataRegistry
{
public:
    FontDataRegistry()
    {
        m_data.fill(nullptr);
    }

    // TODO : Copy data?
    inline void add(uint16_t id, const void* data)
    {
        m_data[id] = data;
    }

    inline const void* operator[](uint16_t id) const
    {
        return m_data[id];
    }

private:
    Array<const void*, MAX_FONTS> m_data;
};

class Atlas
{
public:
    inline float font_size() const { return m_font_size; }

    inline bool is_free() const { return m_flags & ATLAS_FREE; }

    inline bool is_locked() const { return m_locked; }

    inline bool is_updatable() const { return m_flags & ATLAS_ALLOW_UPDATE; }

    inline bool is_monospaced() const { return m_flags & ATLAS_MONOSPACED; }

    inline bool does_not_require_thread_safety() const { return m_flags & ATLAS_NOT_THREAD_SAFE; }

    void reset(uint16_t texture, uint16_t flags, const void* font, float size, TextureCache& textures)
    {
        MutexScope lock(m_mutex);

        if (m_texture != UINT16_MAX)
        {
            textures.destroy_texture(m_texture);
        }

        // NOTE : It'd be much nicer to be able to call `*this = {}`, but we
        //        can't because of the mutex :-(.
        m_requests   .clear();
        m_pack_rects .clear();
        m_pack_nodes .clear();
        m_char_quads .clear();
        m_codepoints .clear();
        m_bitmap_data.clear();

        m_font_info     = {};
        m_pack_ctx      = {};
        m_bitmap_width  = 0;
        m_bitmap_height = 0;
        m_padding       = 1; // TODO : Padding should probably reflect whether an SDF atlas is required.
        m_locked        = false;
        m_font_size     = size;
        m_texture       = texture;
        m_flags         = flags;

        // TODO : Check return value.
        (void)stbtt_InitFont(&m_font_info, static_cast<const uint8_t*>(font), 0);

        if (const int table = stbtt__find_table(m_font_info.data, m_font_info.fontstart, "OS/2"))
        {
            if (ttUSHORT(m_font_info.data + table     ) >= 1 && // Version.
                ttBYTE  (m_font_info.data + table + 32) == 2 && // PANOSE / Kind == `Latin Text`.
                ttBYTE  (m_font_info.data + table + 35) == 9)   // PANOSE / bProportion == "Monospaced"
            {
                flags |= ATLAS_MONOSPACED;
            }
        }
    }

    void add_glyph_range(uint32_t first, uint32_t last)
    {
        if (!is_updatable() && is_locked())
        {
            ASSERT(false && "Atlas is not updatable.");
            return;
        }

        ASSERT(last >= first);

        size_t i = m_requests.size();
        m_requests.resize(i + static_cast<size_t>(last - first + 1));

        for (uint32_t codepoint = first; codepoint <= last; codepoint++, i++)
        {
            if (!m_codepoints.count(codepoint))
            {
                m_requests[i] = codepoint;
            }
        }
    }

    void add_glyphs_from_string(const char* start, const char* end)
    {
        if (!is_updatable() && is_locked())
        {
            ASSERT(false && "Atlas is not updatable.");
            return;
        }

        uint32_t codepoint;
        uint32_t state = 0;

        for (const char* string = start; end ? string < end : *string; string++)
        {
            if (UTF8_ACCEPT == utf8_decode(&state, &codepoint, *reinterpret_cast<const uint8_t*>(string)))
            {
                if (!m_codepoints.count(codepoint))
                {
                    m_requests.push_back(codepoint);
                }
            }
        }

        ASSERT(state == UTF8_ACCEPT);
    }

    // TODO : This should return success bool.
    void update(TextureCache& texture_cache)
    {
        ASSERT(is_updatable() || !is_locked());

        if (m_requests.empty() || is_locked())
        {
            return;
        }

        std::sort(m_requests.begin(), m_requests.end());
        m_requests.erase(std::unique(m_requests.begin(), m_requests.end()), m_requests.end());

        ASSERT(m_pack_rects.size() == m_char_quads.size());

        const size_t count  = m_requests  .size();
        const size_t offset = m_pack_rects.size();

        m_pack_rects.resize(offset + count, stbrp_rect       {});
        m_char_quads.resize(offset + count, stbtt_packedchar {});

        stbtt_pack_context ctx            = {};
        ctx.padding                       = m_padding;
        ctx.h_oversample                  = horizontal_oversampling();
        ctx.v_oversample                  = vertical_oversampling  ();
        ctx.skip_missing                  = 0;

        stbtt_pack_range range            = {};
        range.font_size                   = font_scale();
        range.h_oversample                = horizontal_oversampling();
        range.v_oversample                = vertical_oversampling  ();
        range.chardata_for_range          = m_char_quads.data() + offset;
        range.array_of_unicode_codepoints = reinterpret_cast<int*>(m_requests.data());
        range.num_chars                   = static_cast<int>(m_requests.size());

        (void)stbtt_PackFontRangesGatherRects(
            &ctx,
            &m_font_info,
            &range,
            1,
            m_pack_rects.data() + offset
        );

        uint32_t pack_size[] = { m_bitmap_width, m_bitmap_height };
        pack_rects(offset, count, pack_size);

        if (m_bitmap_width  != pack_size[0] ||
            m_bitmap_height != pack_size[1])
        {
            Vector<uint8_t> data(pack_size[0] * pack_size[1], 0);

            for (uint32_t y = 0, src_offset = 0, dst_offset = 0; y < m_bitmap_height; y++)
            {
                memcpy(data.data() + dst_offset, m_bitmap_data.data() + src_offset, m_bitmap_width);

                src_offset += m_bitmap_width;
                dst_offset += pack_size[0];
            }

            m_bitmap_width  = pack_size[0];
            m_bitmap_height = pack_size[1];
            m_bitmap_data.swap(data);
        }

        ctx.width           = m_bitmap_width;
        ctx.height          = m_bitmap_height;
        ctx.stride_in_bytes = m_bitmap_width;
        ctx.pixels          = m_bitmap_data.data();

        // TODO : Utilize the return value.
        (void)stbtt_PackFontRangesRenderIntoRects(
            &ctx,
            &m_font_info,
            &range,
            1,
            m_pack_rects.data() + offset
        );

        // TODO : We should only update the texture if the size didn't change.
        texture_cache.add_texture(
            m_texture,
            TEXTURE_R8,
            m_bitmap_width,
            m_bitmap_height,
            0,
            m_bitmap_data.data()
        );

        for (size_t i = 0; i < m_requests.size(); i++)
        {
            m_codepoints.insert({ m_requests[i], static_cast<uint16_t>(offset + i) });
        }

        m_requests.clear();

        if (!is_updatable())
        {
            m_locked = true;
        }
    }

    using QuadPackFunc = void (*)(const stbtt_packedchar&, float, float, float&, stbtt_aligned_quad&);

    QuadPackFunc get_quad_pack_func(bool align_to_integer, bool y_axis_down)
    {
        // TODO : Move to class-member level?
        static const QuadPackFunc s_dispatch_table[8] =
        {
            //        +------------------- YAxisDown
            //        |  +---------------- UseTexCoord
            //        |  |  +------------- AlignToInteger
            //        |  |  |
            pack_quad<0, 0, 0>,
            pack_quad<0, 0, 1>,
            pack_quad<0, 1, 0>,
            pack_quad<0, 1, 1>,
            pack_quad<1, 0, 0>,
            pack_quad<1, 0, 1>,
            pack_quad<1, 1, 0>,
            pack_quad<1, 1, 1>,
        };

        const bool use_tex_coord = !is_updatable();
        const int  index         = 
            (align_to_integer ? 0b001 : 0) |
            (use_tex_coord    ? 0b010 : 0) |
            (y_axis_down      ? 0b100 : 0) ;

        return s_dispatch_table[index];
    }

    bool get_text_size
    (
        const char* start,
        const char* end,
        float       line_height_factor,
        float*      out_width,
        float*      out_height
    )
    {
        float       line_width  = 0.0f;
        const float line_height = roundf(font_size() * line_height_factor);
        float       box_width   = 0.0f;
        float       box_height  = font_size();
        uint32_t    codepoint   = 0;
        uint32_t    state       = 0;

        for (const char* string = start; end ? string < end : *string; string++)
        {
            if (UTF8_ACCEPT == utf8_decode(&state, &codepoint, *reinterpret_cast<const uint8_t*>(string)))
            {
                if (codepoint == '\n') // TODO : Other line terminators?
                {
                    box_height += line_height;
                    box_width   = std::max(box_width, line_width);
                    line_width  = 0.0f;

                    continue;
                }

                const auto it = m_codepoints.find(codepoint);

                if (it == m_codepoints.end())
                {
                    return false;
                }

                // TODO : Needs to reflect `align_to_integer`.
                line_width += m_char_quads[it->second].xadvance;
            }
        }

        ASSERT(state == UTF8_ACCEPT);

        box_width = std::max(box_width, line_width);

        if (out_width ) { *out_width  = box_width ; }
        if (out_height) { *out_height = box_height; }

        return true;
    }

    // Two-pass:
    // 1) Gather info about text, signal missing glyphs.
    // 2) Submit quads to the recorder.
    bool lay_text
    (
        const char*   start,
        const char*   end,
        float         line_height_factor,
        uint16_t      h_alignment,
        uint16_t      v_alignment,
        bool          align_to_integer,
        bool          y_axis_down,
        const Mat4&   transform,
        MeshRecorder& out_recorder
    )
    {
        Vector<float> line_widths; // TODO : Candidate for stack-based allocator usage.
        const float   line_sign         = y_axis_down ? 1.0f : -1.0f;
        const float   line_height       = roundf(font_size() * line_height_factor);
        float         line_width        = 0.0f;
        float         box_width         = 0.0f;
        float         box_height        = font_size();
        uint32_t      codepoint         = 0;
        uint32_t      state             = 0;
        const bool    needs_line_widths = h_alignment != TEXT_H_ALIGN_LEFT;

        // Pass 1: Gather info about text, signal missing glyphs.
        for (const char* string = start; end ? string < end : *string; string++)
        {
            if (UTF8_ACCEPT == utf8_decode(&state, &codepoint, *reinterpret_cast<const uint8_t*>(string)))
            {
                if (codepoint == '\n') // TODO : Other line terminators?
                {
                    if (needs_line_widths)
                    {
                        line_widths.push_back(line_width);
                    }

                    box_height += line_height;
                    box_width   = std::max(box_width, line_width);
                    line_width  = 0.0f;

                    continue;
                }

                const auto it = m_codepoints.find(codepoint);

                if (it == m_codepoints.end())
                {
                    if (is_updatable())
                    {
                        return false;
                    }
                    else
                    {
                        // TODO : Probably just print some warning and skip the
                        //        glyph (try using the atlas' "missing" glyph).
                        ASSERT(false && "Atlas is immutable.");
                    }
                }

                // TODO : Needs to reflect `align_to_integer`.
                line_width += m_char_quads[it->second].xadvance;
            }
        }

        ASSERT(state == UTF8_ACCEPT);

        if (needs_line_widths)
        {
            line_widths.push_back(line_width);
        }

        box_width = std::max(box_width, line_width);

        if (box_width == 0.0f)
        {
            return true;
        }

        // Pass 2: Submit quads to the recorder.
        Vec3     offset   = HMM_Vec3(0.0f, 0.0f, 0.0f);
        uint16_t line_idx = 0;

        switch (v_alignment)
        {
        case TEXT_V_ALIGN_BASELINE:
            offset.Y = line_sign * (font_size() - box_height);
            break;
        case TEXT_V_ALIGN_MIDDLE:
            offset.Y = roundf(line_sign * (box_height * -0.5f + font_size()));
            break;
        case TEXT_V_ALIGN_CAP_HEIGHT:
            offset.Y = line_sign * font_size();
            break;
        default:;
        }

        const QuadPackFunc pack_func = get_quad_pack_func(align_to_integer, y_axis_down);

        for (const char* string = start; end ? string < end : *string; line_idx++)
        {
            ASSERT(!end || string < end);

            switch (h_alignment)
            {
            case TEXT_H_ALIGN_CENTER:
                offset.X = line_widths[line_idx] * -0.5f;
                break;
            case TEXT_H_ALIGN_RIGHT:
                offset.X = -line_widths[line_idx];
                break;
            default:;
            }

            string = record_quads
            (
                string,
                end,
                pack_func,
                transform * HMM_Translate(offset),
                out_recorder
            );

            offset.Y += line_sign * line_height;
        }

        return true;
    }

private:
    template <bool YAxisDown, bool UseTexCoord, bool AlignToInteger>
    static void pack_quad
    (
        const stbtt_packedchar& char_info,
        float                   inv_width,
        float                   inv_height,
        float&                  inout_xpos,
        stbtt_aligned_quad&     out_quad
    )
    {
        if constexpr (AlignToInteger)
        {
            const float x = floorf(inout_xpos + char_info.xoff + 0.5f);
            const float y = floorf(             char_info.yoff + 0.5f);

            out_quad.x0 = x;
            out_quad.x1 = x + char_info.xoff2 - char_info.xoff;

            if constexpr (YAxisDown)
            {
                out_quad.y0 = y;
                out_quad.y1 = y + char_info.yoff2 - char_info.yoff;
            }
            else
            {
                out_quad.y0 = -y;
                out_quad.y1 = -y - char_info.yoff2 + char_info.yoff;
            }
        }
        else
        {
            out_quad.x0 = inout_xpos + char_info.xoff;
            out_quad.x1 = inout_xpos + char_info.xoff2;

            if constexpr (YAxisDown)
            {
                out_quad.y0 = char_info.yoff;
                out_quad.y1 = char_info.yoff2;
            }
            else
            {
                out_quad.y0 = -char_info.yoff;
                out_quad.y1 = -char_info.yoff2;
            }
        }

        if constexpr (UseTexCoord)
        {
            out_quad.s0 = char_info.x0 * inv_width;
            out_quad.t0 = char_info.y0 * inv_height;
            out_quad.s1 = char_info.x1 * inv_width;
            out_quad.t1 = char_info.y1 * inv_height;
        }
        else
        {
            out_quad.s0 = char_info.x0;
            out_quad.t0 = char_info.y0;
            out_quad.s1 = char_info.x1;
            out_quad.t1 = char_info.y1;
        }

        inout_xpos += char_info.xadvance;
    }

    inline const char* record_quads(const char* start, const char* end, const QuadPackFunc& pack_func, const Mat4& transform, MeshRecorder& recorder)
    {
        if (!is_updatable() || does_not_require_thread_safety())
        {
            return record_quads_without_lock(start, end, pack_func, transform, recorder);
        }
        else
        {
            MutexScope lock(m_mutex);

            return record_quads_without_lock(start, end, pack_func, transform, recorder);
        }
    }

    const char* record_quads_without_lock(const char* start, const char* end, const QuadPackFunc& pack_func, const Mat4& transform, MeshRecorder& recorder)
    {
        // NOTE : This routine assumes all needed glyphs are loaded!

        uint32_t           codepoint;
        uint32_t           state      = 0;
        const float        inv_width  = 1.0f / static_cast<float>(m_bitmap_width );
        const float        inv_height = 1.0f / static_cast<float>(m_bitmap_height);
        float              x          = 0.0f;
        stbtt_aligned_quad quad       = {};

        for (; end ? start < end : *start; start++)
        {
            if (UTF8_ACCEPT == utf8_decode(&state, &codepoint, *reinterpret_cast<const uint8_t*>(start)))
            {
                if (codepoint == '\n') // TODO : Other line terminators?
                {
                    start++;
                    break;
                }

                const auto it = m_codepoints.find(codepoint);
                ASSERT(it != m_codepoints.end());

                (*pack_func)(m_char_quads[it->second], inv_width, inv_height, x, quad);

                recorder.texcoord(                      quad.s0, quad.t0);
                recorder.vertex  ((transform * HMM_Vec4(quad.x0, quad.y0, 0.0f, 1.0f)).XYZ);

                recorder.texcoord(                      quad.s0, quad.t1);
                recorder.vertex  ((transform * HMM_Vec4(quad.x0, quad.y1, 0.0f, 1.0f)).XYZ);

                recorder.texcoord(                      quad.s1, quad.t1);
                recorder.vertex  ((transform * HMM_Vec4(quad.x1, quad.y1, 0.0f, 1.0f)).XYZ);

                recorder.texcoord(                      quad.s1, quad.t0);
                recorder.vertex  ((transform * HMM_Vec4(quad.x1, quad.y0, 0.0f, 1.0f)).XYZ);
            }
        }

        ASSERT(state == UTF8_ACCEPT);

        return start;
    }

    int16_t cap_height() const
    {
        if (const int table = stbtt__find_table(m_font_info.data, m_font_info.fontstart, "OS/2"))
        {
            if (ttUSHORT(m_font_info.data + table) >= 2) // Version.
            {
                return ttSHORT(m_font_info.data + table + 88); // sCapHeight.
            }
        }

        // TODO : Estimate cap height from capital `H` bounding box?
        ASSERT(false && "Can't determine cap height.");

        return 0;
    }

    inline float font_scale() const
    {
        int ascent, descent;
        stbtt_GetFontVMetrics(&m_font_info, &ascent, &descent, nullptr);

        return (ascent - descent) * m_font_size / cap_height();
    }

    inline uint8_t horizontal_oversampling() const
    {
        constexpr uint32_t H_OVERSAMPLE_MASK  = ATLAS_H_OVERSAMPLE_2X | ATLAS_H_OVERSAMPLE_3X | ATLAS_H_OVERSAMPLE_4X;
        constexpr uint32_t H_OVERSAMPLE_SHIFT = 3;

        const uint8_t value = static_cast<uint8_t>(((m_flags & H_OVERSAMPLE_MASK) >> H_OVERSAMPLE_SHIFT) + 1);
        ASSERT(value >= 1 && value <= 4);

        return value;
    }

    inline uint8_t vertical_oversampling() const
    {
        constexpr uint32_t V_OVERSAMPLE_MASK  = ATLAS_V_OVERSAMPLE_2X;
        constexpr uint32_t V_OVERSAMPLE_SHIFT = 6;

        const uint8_t value = static_cast<uint8_t>(((m_flags & V_OVERSAMPLE_MASK) >> V_OVERSAMPLE_SHIFT) + 1);
        ASSERT(value >= 1 && value <= 2);

        return value;
    }

    bool pick_next_size(uint32_t min_area, uint32_t* inout_pack_size) const
    {
        const uint32_t max_size = bgfx::getCaps()->limits.maxTextureSize;
        uint32_t       size[2]  = { 64, 64 };

        for (int j = 0;; j = (j + 1) % 2)
        {
            if (size[0] > inout_pack_size[0] || size[1] > inout_pack_size[1])
            {
                const uint32_t area = (size[0] - m_padding) * (size[1] - m_padding);

                if (area >= min_area * 1.075f) // 7.5 % extra space, as the packing won't be perfect.
                {
                    break;
                }
            }

            if (size[0] == max_size && size[1] == max_size)
            {
                ASSERT(false && "Maximum atlas size reached."); // TODO : Convert to `WARNING`.
                return false;
            }

            size[j] *= 2;
        }

        inout_pack_size[0] = size[0];
        inout_pack_size[1] = size[1];

        return true;
    }

    void pack_rects(size_t offset, size_t count, uint32_t* inout_pack_size)
    {
        uint32_t    min_area   = 0;
        const float extra_area = 1.05f;

        for (const stbrp_rect& rect : m_pack_rects)
        {
            min_area += static_cast<uint32_t>(rect.w * rect.h);
        }

        min_area = static_cast<uint32_t>(static_cast<float>(min_area) * extra_area);

        for (;;)
        {
            if (inout_pack_size[0] > 0 && inout_pack_size[1] > 0)
            {
                // TODO : It's probably possible to revert the packing context
                //        without having to making its full copy beforehand.
                stbrp_context      ctx = m_pack_ctx;
                Vector<stbrp_node> nodes(m_pack_nodes); // TODO : Candidate for stack-based allocator usage.

                // NOTE : This only packs the new rectangles.
                if (1 == stbrp_pack_rects(
                    &m_pack_ctx,
                    m_pack_rects.data() + offset,
                    static_cast<int>(count)
                ))
                {
                    break;
                }
                else
                {
                    m_pack_ctx   = ctx;
                    m_pack_nodes = nodes;
                }

                // TODO : We could adjust `offset` and `count` so that the rects
                //        that were successfully packed would be skipped in next
                //        resizing attempt, but we'd have to reorder them.
            }

            if (pick_next_size(min_area, inout_pack_size))
            {
                if (m_pack_ctx.num_nodes == 0)
                {
                    m_pack_nodes.resize(inout_pack_size[0] - m_padding);

                    stbrp_init_target(
                        &m_pack_ctx,
                        inout_pack_size[0] - m_padding,
                        inout_pack_size[1] - m_padding,
                        m_pack_nodes.data(),
                        static_cast<int>(m_pack_nodes.size())
                    );
                }
                else
                {
                    // Atlas size changed (and so did the packing rectangle).
                    patch_stbrp_context(inout_pack_size[0], inout_pack_size[1]);
                }
            }
            else
            {
                ASSERT(false && "Maximum atlas size reached and all glyphs "
                    "still can't be packed."); // TODO : Convert to `WARNING`.
                break;
            }
        }
    }

#ifndef NDEBUG
    static void check_stbrp_context_validity(const stbrp_context& ctx, const Vector<stbrp_node>& nodes)
    {
        const auto check_node =[&](const stbrp_node* node)
        {
            const bool is_in_range = node >= nodes.data() && node < nodes.data() + nodes.size();
            const bool is_extra    = node == &ctx.extra[0] || node == &ctx.extra[1];
            const bool is_null     = node == nullptr;

            ASSERT(is_in_range || is_extra || is_null);
        };

        const auto count_nodes = [](const stbrp_node* node, bool check_zero)
        {
            int count = 0;

            while (node)
            {
                ASSERT(!check_zero || (node->x == 0 && node->y == 0));

                node = node->next;
                count++;
            }

            return count;
        };

        const int active_nodes_count = count_nodes(ctx.active_head, false);
        const int free_nodes_count   = count_nodes(ctx.free_head  , true );

        ASSERT(2 + ctx.num_nodes == active_nodes_count + free_nodes_count);

        check_node(ctx.active_head);
        check_node(ctx.active_head->next);

        check_node(ctx.free_head);
        check_node(ctx.free_head->next);

        check_node(ctx.extra[0].next);
        check_node(ctx.extra[1].next);

        for (auto& node : nodes)
        {
            check_node(node.next);
        }
    }
#endif // NDEBUG

    void patch_stbrp_context(uint32_t width, uint32_t height)
    {
#ifndef NDEBUG
        check_stbrp_context_validity(m_pack_ctx, m_pack_nodes);
#endif

        // When changing only height, number of nodes or the sentinel node don't
        // change.
        if (width - m_padding == static_cast<uint32_t>(m_pack_ctx.width))
        {
            m_pack_ctx.height = static_cast<int>(height - m_padding);

            return;
        }

        stbrp_context      ctx = {};
        Vector<stbrp_node> nodes(width - m_padding);

        const auto find_node = [&](stbrp_node* node)
        {
            // Node can either point to one of the given array members, the two
            // `extra` nodes allocated within the `stbrp_context` structure, or
            // be `NULL`.
            const uintptr_t offset = node - m_pack_nodes.data(); // Fine even if `nullptr`.

            // We're intentionally not adjusting the nodes that point to one of
            // the context's `extra` nodes, so that we don't have to repeatedly
            // patch them when the context would be swapped.
            return offset < m_pack_nodes.size()
                ? &nodes[offset]
                : node;
        };

        stbrp_init_target(
            &ctx,
            static_cast<int>(width  - m_padding),
            static_cast<int>(height - m_padding),
            nodes.data(),
            static_cast<int>(nodes.size())
        );

        ctx.active_head   = find_node(m_pack_ctx.active_head  );
        ctx.free_head     = find_node(m_pack_ctx.free_head    );
        ctx.extra[0].next = find_node(m_pack_ctx.extra[0].next);
        ctx.extra[0].x    =           m_pack_ctx.extra[0].x    ;
        ctx.extra[0].y    =           m_pack_ctx.extra[0].y    ;
        // NOTE : Node `extra[1]` is a sentinel, so no need to patch it.

        // TODO : It's possible that some special handling will be necessary if
        //        the old context ran out of free nodes, but we're increasing
        //        width. So do some special logic here (will need artifical test
        //        as it's quite unlikely, I think).
        if (!m_pack_ctx.free_head)
        {
            ASSERT(false && "Not implemented.");
        }

        for (size_t i = 0; i < m_pack_nodes.size() - 1; i++)
        {
            nodes[i].x    =           m_pack_nodes[i].x;
            nodes[i].y    =           m_pack_nodes[i].y;
            nodes[i].next = find_node(m_pack_nodes[i].next);
        }

        m_pack_ctx = ctx; // We can do this safely as no nodes point to `ctx.extra`.
        m_pack_nodes.swap(nodes);

#ifndef NDEBUG
        check_stbrp_context_validity(m_pack_ctx, m_pack_nodes);
#endif
    }

private:
    Mutex                       m_mutex;

    stbtt_fontinfo              m_font_info     = {};
    float                       m_font_size     = 0.0f; // Cap height, in pixels.

    Vector<uint32_t>            m_requests;

    // Packing.
    stbrp_context               m_pack_ctx      = {};
    Vector<stbrp_rect>          m_pack_rects;
    Vector<stbrp_node>          m_pack_nodes;

    // Packed data.
    Vector<stbtt_packedchar>    m_char_quads;
    HashMap<uint32_t, uint16_t> m_codepoints;

    // Bitmap.
    Vector<uint8_t>             m_bitmap_data;
    uint16_t                    m_bitmap_width  = 0;
    uint16_t                    m_bitmap_height = 0;

    uint16_t                    m_texture       = UINT16_MAX;
    uint16_t                    m_flags         = ATLAS_FREE;
    uint8_t                     m_padding       = 1;
    bool                        m_locked        = false;
};

class AtlasCache
{
public:
    AtlasCache()
    {
        m_indices.fill(UINT16_MAX);
    }

    inline Atlas* get(uint16_t id)
    {
        return m_indices[id] != UINT16_MAX ? &m_atlases[m_indices[id]] : nullptr;
    }

    Atlas* get_or_create(uint16_t id)
    {
        if (m_indices[id] != UINT16_MAX)
        {
            return &m_atlases[m_indices[id]];
        }

        MutexScope lock(m_mutex);

        for (size_t i = 0; i < m_atlases.size(); i++)
        {
            if (m_atlases[i].is_free())
            {
                m_indices[id] = static_cast<uint16_t>(i);

                return &m_atlases[i];
            }
        }

        ASSERT(false && "No more free atlas slots.");

        return nullptr;
    }

private:
    Mutex                             m_mutex;
    Array<Atlas, MAX_TEXTURE_ATLASES> m_atlases;
    Array<uint16_t, MAX_TEXTURES>     m_indices;
};


// -----------------------------------------------------------------------------
// TEXT MESH RECORDING
// -----------------------------------------------------------------------------

class TextRecorder
{
public:
    void begin(uint16_t id, uint16_t flags, uint16_t atlas_id, Atlas* atlas, MeshRecorder* recorder)
    {
        ASSERT(!m_atlas);
        ASSERT(!m_recorder);

        ASSERT(atlas);
        ASSERT(recorder);
        ASSERT(!recorder->is_recording());

        const uint32_t mesh_flags =
            TEXT_MESH       |
            PRIMITIVE_QUADS |
            VERTEX_POSITION |
            VERTEX_TEXCOORD |
            VERTEX_COLOR    |
            (atlas->is_updatable() ? (TEXCOORD_F32 | VERTEX_PIXCOORD) : 0) |
            (flags & TEXT_TYPE_MASK);

        m_flags       = flags;
        m_atlas       = atlas;
        m_recorder    = recorder;
        m_line_height = 2.0f;

        m_recorder->begin(id, mesh_flags, atlas_id);
    }

    void end()
    {
        ASSERT(m_recorder);

        m_recorder->end();

        *this = {};
    }

    void set_alignment(uint16_t flags)
    {
        ASSERT(m_recorder);

        if (flags & TEXT_H_ALIGN_MASK)
        {
            m_flags = (m_flags & ~TEXT_H_ALIGN_MASK) | (flags & TEXT_H_ALIGN_MASK);
        }

        if (flags & TEXT_V_ALIGN_MASK)
        {
            m_flags = (m_flags & ~TEXT_V_ALIGN_MASK) | (flags & TEXT_V_ALIGN_MASK);
        }
    }

    void set_line_height(float factor)
    {
        ASSERT(m_recorder);

        m_line_height = factor;
    }

    void add_text(const char* start, const char* end, const Mat4& transform, TextureCache& inout_texture_cache)
    {
        ASSERT(m_recorder);

        const auto lay_text = [&]()
        {
            return m_atlas->lay_text(
                start,
                end,
                m_line_height,
                h_alignment(),
                v_alignment(),
                align_to_integer(),
                y_axis() == TEXT_Y_AXIS_DOWN,
                transform,
                *m_recorder
            );
        };

        if (!lay_text() && m_atlas->is_updatable())
        {
            m_atlas->add_glyphs_from_string(start, end);
            m_atlas->update(inout_texture_cache);

            const bool success = lay_text();
            BX_UNUSED (success);
            ASSERT    (success);
        }
    }

    inline const MeshRecorder* mesh_recorder() const
    {
        return m_recorder;
    }

private:
    inline uint16_t h_alignment() const
    {
        constexpr uint16_t alignment[] =
        {
            TEXT_H_ALIGN_LEFT  ,
            TEXT_H_ALIGN_CENTER,
            TEXT_H_ALIGN_RIGHT ,
        };

        return alignment[(m_flags & TEXT_H_ALIGN_MASK) >> TEXT_H_ALIGN_SHIFT];
    }

    inline uint16_t v_alignment() const
    {
        constexpr uint16_t alignment[] =
        {
            TEXT_V_ALIGN_BASELINE  ,
            TEXT_V_ALIGN_MIDDLE    ,
            TEXT_V_ALIGN_CAP_HEIGHT,
        };

        return alignment[(m_flags & TEXT_V_ALIGN_MASK) >> TEXT_V_ALIGN_SHIFT];
    }

    inline uint16_t y_axis() const
    {
        constexpr uint16_t alignment[] =
        {
            TEXT_Y_AXIS_DOWN,
            TEXT_Y_AXIS_UP  ,
        };

        return alignment[(m_flags & TEXT_Y_AXIS_MASK) >> TEXT_Y_AXIS_SHIFT];
    }

    inline bool align_to_integer() const
    {
        return m_flags & TEXT_ALIGN_TO_INTEGER;
    }

private:
    MeshRecorder* m_recorder    = nullptr;
    Atlas*        m_atlas       = nullptr;
    float         m_line_height = 2.0f;
    uint16_t      m_flags       = 0;
};


// -----------------------------------------------------------------------------
// TIME MEASUREMENT
// -----------------------------------------------------------------------------

struct Timer 
{
    i64 counter = 0;
    f64 elapsed = 0;
    f64 frequency = f64(bx::getHPFrequency());

    inline void tic()
    {
        counter = bx::getHPCounter();
    }

    f64 toc(bool restart = false)
    {
        const i64 now = bx::getHPCounter();

        elapsed = (now - counter) / frequency;

        if (restart)
        {
            counter = now;
        }

        return elapsed;
    }
};


// -----------------------------------------------------------------------------
// WINDOW
// -----------------------------------------------------------------------------

struct Window
{
    GLFWwindow* handle                  = nullptr;

    int         framebuffer_width       = 0;
    int         framebuffer_height      = 0;

    float       dpi_invariant_width     = 0.0f;
    float       dpi_invariant_height    = 0.0f;

    float       position_scale_x        = 0.0f;
    float       position_scale_y        = 0.0f;

    float       display_scale_x         = 0.0f;
    float       display_scale_y         = 0.0f;
    bool        display_scale_x_changed = false;

    void update_size_info()
    {
        ASSERT(handle);

        int         window_width         = 0;
        int         window_height        = 0;
        const float prev_display_scale_x = display_scale_x;

        glfwGetWindowSize        (handle, &window_width     , &window_height     );
        glfwGetFramebufferSize   (handle, &framebuffer_width, &framebuffer_height);
        glfwGetWindowContentScale(handle, &display_scale_x  , &display_scale_y   );

        display_scale_x_changed = display_scale_x != prev_display_scale_x;

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

    ASSERT(window);
    ASSERT(flags >= 0);

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
    enum Flag : uint8_t
    {
        DOWN     = 0x01,
        UP       = 0x02,
        HELD     = 0x04,
        REPEATED = 0x08,
    };

    static constexpr int INPUT_COUNT            = MAX_INPUTS;
    static constexpr int INVALID_INPUT          =  -1;

    uint8_t              states    [MAX_INPUTS] = { 0    };
    float                timestamps[MAX_INPUTS] = { 0.0f };

    inline bool is(int app_input, int flag) const
    {
        const int input = T::translate_app_input(app_input);

        return (BX_LIKELY(input > INVALID_INPUT && input < MAX_INPUTS))
            ? states[input] & flag
            : false;
    }

    inline float held_time(int app_input, float timestamp) const
    {
        const int input = T::translate_app_input(app_input);

        if (BX_LIKELY(input > INVALID_INPUT && input < MAX_INPUTS))
        {
            if (states[input] & (DOWN | HELD))
            {
                ASSERT(timestamp >= timestamps[input]);
                return timestamp -  timestamps[input];
            }
        }

        return -1.0f;
    }

    void update_input_state(int input, Flag flag, float timestamp = 0.0f)
    {
        if (BX_LIKELY(input > INVALID_INPUT && input < MAX_INPUTS))
        {
            states[input] |= flag;

            if (flag == DOWN)
            {
                timestamps[input] = timestamp;
            }
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
            else
            {
                states[i] &= ~REPEATED;
            }
        }
    }
};

struct Mouse : InputState<GLFW_MOUSE_BUTTON_LAST, Mouse>
{
    static constexpr float REPEATED_CLICK_DELAY = 0.5f; // NOTE : Could be configurable.

    float                  curr  [2]            = { 0.0f };
    float                  prev  [2]            = { 0.0f };
    float                  delta [2]            = { 0.0f };
    float                  scroll[2]            = { 0.0f };
    int                    clicks[INPUT_COUNT]  = { 0    };

    inline int repeated_click_count(int app_input) const
    {
        const int input = translate_app_input(app_input);

        if (BX_LIKELY(input > INVALID_INPUT && input < INPUT_COUNT))
        {
            return (states[input] & DOWN)
                ? clicks[input]
                : 0;
        }

        return 0;
    }

    void update_input_state(int input, Flag flag, float timestamp = 0.0f)
    {
        if (BX_LIKELY(input > INVALID_INPUT && input < INPUT_COUNT))
        {
            states[input] |= flag;

            if (flag == DOWN)
            {
                if (timestamp - timestamps[input] <= REPEATED_CLICK_DELAY)
                {
                    clicks[input]++;
                }
                else
                {
                    clicks[input] = 1;
                }

                timestamps[input] = timestamp;
            }
        }
    }

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
            0,                     // KEY_ANY

            GLFW_KEY_LEFT_ALT,     // KEY_ALT_LEFT
            GLFW_KEY_RIGHT_ALT,    // KEY_ALT_RIGHT
            GLFW_KEY_BACKSPACE,    // KEY_BACKSPACE
            GLFW_KEY_LEFT_CONTROL, // KEY_CONTROL_LEFT
            GLFW_KEY_RIGHT_CONTROL, // KEY_CONTROL_RIGHT
            GLFW_KEY_DELETE,        // KEY_DELETE
            GLFW_KEY_DOWN,          // KEY_DOWN
            GLFW_KEY_ENTER,         // KEY_ENTER
            GLFW_KEY_ESCAPE,        // KEY_ESCAPE
            GLFW_KEY_LEFT,          // KEY_LEFT
            GLFW_KEY_RIGHT,         // KEY_RIGHT
            GLFW_KEY_LEFT_SHIFT,    // KEY_SHIFT_LEFT
            GLFW_KEY_RIGHT_SHIFT,   // KEY_SHIFT_RIGHT
            GLFW_KEY_SPACE,         // KEY_SPACE
            GLFW_KEY_LEFT_SUPER,    // KEY_SUPER_LEFT
            GLFW_KEY_RIGHT_SUPER,   // KEY_SUPER_RIGHT
            GLFW_KEY_TAB,           // KEY_TAB
            GLFW_KEY_UP,            // KEY_UP

            GLFW_KEY_F1,            // KEY_F1
            GLFW_KEY_F2,            // KEY_F2
            GLFW_KEY_F3,            // KEY_F3
            GLFW_KEY_F4,            // KEY_F4
            GLFW_KEY_F5,            // KEY_F5
            GLFW_KEY_F6,            // KEY_F6
            GLFW_KEY_F7,            // KEY_F7
            GLFW_KEY_F8,            // KEY_F8
            GLFW_KEY_F9,            // KEY_F9
            GLFW_KEY_F10,           // KEY_F10
            GLFW_KEY_F11,           // KEY_F11
            GLFW_KEY_F12,           // KEY_F12
        };

        int glfw_key = INVALID_INPUT;

        if (app_key >= 0 && app_key < static_cast<int>(BX_COUNTOF(special_app_keys)))
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

class TaskPool;

struct Task : enki::ITaskSet
{
    void    (*func)(void*) = nullptr;
    void*     data         = nullptr;
    TaskPool* pool         = nullptr;

    void ExecuteRange(enki::TaskSetPartition, uint32_t) override;
};

class TaskPool
{
public:
    TaskPool()
    {
        for (uint8_t i = 0; i < MAX_TASKS; i++)
        {
            m_tasks[i].pool = this;
            m_nexts[i]      = i + 1;
        }
    }

    Task* get_free_task()
    {
        MutexScope lock(m_mutex);

        Task* task = nullptr;

        if (m_head < MAX_TASKS)
        {
            const uint32_t i = m_head;

            task       = &m_tasks[i];
            m_head     =  m_nexts[i];
            m_nexts[i] = MAX_TASKS;
        }

        return task;
    }

    void release_task(const Task* task)
    {
        ASSERT(task);
        ASSERT(task >= &m_tasks[0] && task <= &m_tasks[MAX_TASKS - 1]);

        MutexScope lock(m_mutex);

        const ptrdiff_t i = task - &m_tasks[0];

        m_tasks[i].func = nullptr;
        m_tasks[i].data = nullptr;
        m_nexts[i]      = m_head;
        m_head          = static_cast<uint8_t>(i);
    }

private:
    Mutex   m_mutex;
    Task    m_tasks[MAX_TASKS];
    uint8_t m_nexts[MAX_TASKS];
    uint8_t m_head = 0;

    static_assert(MAX_TASKS <= UINT8_MAX, "MAX_TASKS too big, change the type.");
};

void Task::ExecuteRange(enki::TaskSetPartition, uint32_t)
{
    // ASSERT(t_ctx);

    ASSERT(func);
    (*func)(data);

    ASSERT(pool);
    pool->release_task(this);
}


// -----------------------------------------------------------------------------
// MEMORY CACHE
// -----------------------------------------------------------------------------

class MemoryCache
{
public:
    void clear()
    {
        MutexScope lock(m_mutex);

        m_contents.clear();
    }

    unsigned char* load_bytes(const char* file_name, int* bytes_read)
    {
        return static_cast<unsigned char*>(load(BYTES, file_name, bytes_read));
    }

    char* load_string(const char* file_name)
    {
        return static_cast<char*>(load(STRING, file_name));
    }

    unsigned char* load_image(const char* file_name, int channels, int* width, int* height)
    {
        int      image_width  = 0;
        int      image_height = 0;
        uint8_t* image_data   = stbi_load(file_name, &image_width, &image_height, nullptr, channels);

        if (!image_data)
        {
            return nullptr;
        }

        if (width)
        {
            *width = image_width;
        }

        if (height)
        {
            *height = image_height;
        }

        Vector<uint8_t> buffer(image_data, image_data + image_width * image_height * channels);
        stbi_image_free(image_data);

        unsigned char* content = buffer.data();

        {
            MutexScope lock(m_mutex);
            m_contents.insert({ content, std::move(buffer) });
        }

        return content;
    }

    unsigned char* decode_image(const unsigned char* data, int bytes, int channels, int* width, int* height)
    {
        int      image_width  = 0;
        int      image_height = 0;
        uint8_t* image_data   = stbi_load_from_memory(data, bytes, &image_width, &image_height, nullptr, channels);

        if (!image_data)
        {
            return nullptr;
        }

        if (width)
        {
            *width = image_width;
        }

        if (height)
        {
            *height = image_height;
        }

        Vector<uint8_t> buffer(image_data, image_data + image_width * image_height * channels);
        stbi_image_free(image_data);

        unsigned char* content = buffer.data();

        {
            MutexScope lock(m_mutex);
            m_contents.insert({ content, std::move(buffer) });
        }

        return content;
    }

    void unload(void* file_content)
    {
        if (file_content)
        {
            MutexScope lock(m_mutex);

            m_contents.erase(file_content);
        }
    }

private:
    enum Type
    {
        BYTES,
        STRING,
    };

    void* load(Type type, const char* file_name, int* bytes_read = nullptr)
    {
        void* content = nullptr;

        if (file_name)
        {
            if (FILE* f = fopen(file_name, "rb"))
            {
                fseek(f, 0, SEEK_END);
                const long length = ftell(f);
                fseek(f, 0, SEEK_SET);

                Vector<uint8_t> buffer(length + (type == STRING));

                if (fread(buffer.data(), 1, length, f) == static_cast<size_t>(length))
                {
                    if (type == STRING)
                    {
                        buffer.back() = 0;
                    }

                    content = buffer.data();

                    if (bytes_read)
                    {
                        *bytes_read = static_cast<int>(length);
                    }

                    {
                        MutexScope lock(m_mutex);
                        m_contents.insert({ content, std::move(buffer) });
                    }
                }
                else
                {
                    ASSERT(false && "File content reading failed.");
                }
            }
        }

        return content;
    }

private:
    Mutex                           m_mutex;
    HashMap<void*, Vector<uint8_t>> m_contents;
};


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
// CONTEXTS
// -----------------------------------------------------------------------------

struct GlobalContext
{
    Keyboard            keyboard;
    Mouse               mouse;

    enki::TaskScheduler task_scheduler;
    TaskPool            task_pool;

    PassCache           pass_cache;
    MeshCache           mesh_cache;
    InstanceCache       instance_cache;
    FramebufferCache    framebuffer_cache;
    ProgramCache        program_cache;
    AtlasCache          atlas_cache;
    TextureCache        texture_cache;
    VertexLayoutCache   layout_cache;
    DefaultUniforms     default_uniforms;
    UniformCache        uniform_cache;
    MemoryCache         memory_cache;
    FontDataRegistry    font_data_registry;
    Vector<uint32_t>    codepoint_queue; // TODO : Make thread safe.

    GLFWcursor*         cursors[6] = { nullptr };
    Window              window;

    Timer               total_time;
    Timer               frame_time;

    int                 active_cursor     = 0;
    uint32_t            frame_number      = 0;
    uint32_t            bgfx_frame_number = 0;

    Atomic<int>         transient_memory  = 32 << 20;

    Atomic<bool>        vsync_on          = false;
    bool                reset_back_buffer = true;
};

struct LocalContext
{
    MeshRecorder        mesh_recorder;
    TextRecorder        text_recorder;
    InstanceRecorder    instance_recorder;

    FramebufferRecorder framebuffer_recorder;

    DrawState           draw_state;

    MatrixStack         matrix_stack;

    Timer               stop_watch;
    Timer               frame_time;

    bgfx::Encoder*      encoder             = nullptr;

    Atlas*              active_atlas        = nullptr;

    bgfx::ViewId        active_pass         = 0;

    bool                is_main_thread      = false;
};

static GlobalContext g_ctx;

static Vector<LocalContext> t_ctxs;

thread_local LocalContext* t_ctx = nullptr;


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY (C++)
// -----------------------------------------------------------------------------

int run(void (* init)(void), void (*setup)(void), void (*draw)(void), void (*cleanup)(void))
{
    // TODO : Check we're not being called multiple times witohut first terminating.
    // TODO : Reset global context data (thread local as well, if possible, but might not be).
    // TODO : Add GLFW error callback and exit `mnm_run` if an error occurrs.
    // TODO : Move thread-local context creations (or at least the pointer setups) before the `init` function

    MNM_TRACE("Starting.");

    if (init)
    {
        (*init)();
    }

    if (glfwInit() != GLFW_TRUE)
    {
        return 1;
    }

    gleqInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // Note that this will be ignored when `glfwSetWindowSize` is specified.

    g_ctx.window.handle = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "MiNiMo", nullptr, nullptr);

    if (!g_ctx.window.handle)
    {
        glfwTerminate();
        return 2;
    }

    g_ctx.window.update_size_info();

    gleqTrackWindow(g_ctx.window.handle);

    {
        // TODO : Set Limits on number of encoders and transient memory.
        // TODO : Init resolution is needed for any backbuffer-size-related
        //        object creations in `setup` function. We should probably just
        //        call the code in the block exectured when
        //        `g_ctx.reset_back_buffer` is true.
        bgfx::Init init;
        init.platformData           = create_platform_data (g_ctx.window.handle, init.type );
        init.resolution.width       = static_cast<uint32_t>(g_ctx.window.framebuffer_width );
        init.resolution.height      = static_cast<uint32_t>(g_ctx.window.framebuffer_height);
        init.limits.transientVbSize = static_cast<uint32_t>(g_ctx.transient_memory         );

        if (!bgfx::init(init))
        {
            glfwDestroyWindow(g_ctx.window.handle);
            glfwTerminate();
            return 3;
        }
    }

    g_ctx.cursors[CURSOR_ARROW    ] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR    );
    g_ctx.cursors[CURSOR_CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    g_ctx.cursors[CURSOR_H_RESIZE ] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR  );
    g_ctx.cursors[CURSOR_HAND     ] = glfwCreateStandardCursor(GLFW_HAND_CURSOR     );
    g_ctx.cursors[CURSOR_I_BEAM   ] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR    );
    g_ctx.cursors[CURSOR_V_RESIZE ] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR  );

    g_ctx.task_scheduler.Initialize(std::max(3u, std::thread::hardware_concurrency()) - 1);

    {
        struct PinnedTask : enki::IPinnedTask
        {
            PinnedTask(uint32_t idx)
                : enki::IPinnedTask(idx)
            {
            }

            void Execute() override
            {
                ASSERT(t_ctx == nullptr);
                ASSERT(threadNum < t_ctxs.size());

                t_ctx = &t_ctxs[threadNum];
            }
        };

        t_ctxs.resize(g_ctx.task_scheduler.GetNumTaskThreads());
        t_ctx = &t_ctxs[0];

        for (uint32_t i = 0; i < g_ctx.task_scheduler.GetNumTaskThreads(); i++)
        {
            t_ctxs[i].is_main_thread = i == 0;

            if (i)
            {
                PinnedTask task(i);
                g_ctx.task_scheduler.AddPinnedTask(&task);
                g_ctx.task_scheduler.WaitforTask(&task);
            }
        }
    }

    g_ctx.layout_cache.init();

    if (setup)
    {
        (*setup)();
    }

    g_ctx.bgfx_frame_number = bgfx::frame();

    uint32_t debug_state = BGFX_DEBUG_NONE;
    bgfx::setDebug(debug_state);

    const bgfx::RendererType::Enum    type        = bgfx::getRendererType();
    static const bgfx::EmbeddedShader s_shaders[] =
    {
        BGFX_EMBEDDED_SHADER(position_fs                 ),
        BGFX_EMBEDDED_SHADER(position_vs                 ),

        BGFX_EMBEDDED_SHADER(position_color_fs           ),
        BGFX_EMBEDDED_SHADER(position_color_vs           ),

        BGFX_EMBEDDED_SHADER(position_color_texcoord_fs  ),
        BGFX_EMBEDDED_SHADER(position_color_texcoord_vs  ),

        BGFX_EMBEDDED_SHADER(position_texcoord_fs        ),
        BGFX_EMBEDDED_SHADER(position_texcoord_vs        ),

        BGFX_EMBEDDED_SHADER(position_color_r_texcoord_fs),
        BGFX_EMBEDDED_SHADER(position_color_r_pixcoord_fs),

        BGFX_EMBEDDED_SHADER(instancing_position_color_vs),

        BGFX_EMBEDDED_SHADER_END()
    };

    {
        const struct
        {
            uint32_t    attribs;
            const char* vs_name;
            const char* fs_name = nullptr;
        }
        programs[] =
        {
            {
                VERTEX_POSITION, // Position only. It's assumed everywhere else.
                "position"
            },
            {
                VERTEX_COLOR,
                "position_color"
            },
            {
                VERTEX_COLOR | VERTEX_TEXCOORD,
                "position_color_texcoord"
            },
            {
                VERTEX_TEXCOORD,
                "position_texcoord"
            },
            {
                VERTEX_COLOR | INSTANCING_SUPPORTED,
                "instancing_position_color",
                "position_color"
            },
            {
                VERTEX_COLOR | VERTEX_TEXCOORD | SAMPLER_COLOR_R,
                "position_color_texcoord",
                "position_color_r_texcoord"
            },
            {
                VERTEX_COLOR | VERTEX_TEXCOORD | VERTEX_PIXCOORD | SAMPLER_COLOR_R,
                "position_color_texcoord",
                "position_color_r_pixcoord"
            },
        };

        char vs_name[32];
        char fs_name[32];

        for (size_t i = 0; i < BX_COUNTOF(programs); i++)
        {
            strcpy(vs_name, programs[i].vs_name);
            strcat(vs_name, "_vs");

            strcpy(fs_name, programs[i].fs_name ? programs[i].fs_name : programs[i].vs_name);
            strcat(fs_name, "_fs");

            (void)g_ctx.program_cache.add(UINT16_MAX, s_shaders, type, vs_name, fs_name, programs[i].attribs);
        }
    }

    g_ctx.default_uniforms.init();

    g_ctx.pass_cache[0].set_viewport(0, 0, SIZE_EQUAL, SIZE_EQUAL);

    g_ctx.mouse.update_position(g_ctx.window);

    g_ctx.total_time.tic();
    g_ctx.frame_time.tic();

    g_ctx.frame_number = 0;

    while (!glfwWindowShouldClose(g_ctx.window.handle))
    {
        g_ctx.keyboard.update_state_flags();
        g_ctx.mouse   .update_state_flags();

        g_ctx.total_time.toc();
        g_ctx.frame_time.toc(true);

        g_ctx.codepoint_queue.clear();

        glfwPollEvents();

        bool   update_cursor_position = false;
        double scroll_accumulator[2]  = { 0.0, 0.0 }; // NOTE : Not sure if we can get multiple scroll events in a single frame.

        GLEQevent event;
        while (gleqNextEvent(&event))
        {
            switch (event.type)
            {
            case GLEQ_KEY_PRESSED:
                g_ctx.keyboard.update_input_state(event.keyboard.key, Keyboard::DOWN, static_cast<float>(g_ctx.total_time.elapsed));
                break;

            case GLEQ_KEY_REPEATED:
                g_ctx.keyboard.update_input_state(event.keyboard.key, Keyboard::REPEATED);
                break;

            case GLEQ_KEY_RELEASED:
                g_ctx.keyboard.update_input_state(event.keyboard.key, Keyboard::UP);
                break;

            case GLEQ_BUTTON_PRESSED:
                g_ctx.mouse.update_input_state(event.mouse.button, Mouse::DOWN, static_cast<float>(g_ctx.total_time.elapsed));
                break;

            case GLEQ_BUTTON_RELEASED:
                g_ctx.mouse.update_input_state(event.mouse.button, Mouse::UP);
                break;

            case GLEQ_CURSOR_MOVED:
                update_cursor_position = true;
                break;

            case GLEQ_SCROLLED:
                scroll_accumulator[0] += event.scroll.x;
                scroll_accumulator[1] += event.scroll.y;
                break;

            case GLEQ_CODEPOINT_INPUT:
                g_ctx.codepoint_queue.push_back(event.codepoint);
                break;

            case GLEQ_FRAMEBUFFER_RESIZED:
            case GLEQ_WINDOW_SCALE_CHANGED:
                g_ctx.reset_back_buffer = true;
                break;

            default:;
                break;
            }

            gleqFreeEvent(&event);
        }

        g_ctx.mouse.scroll[0] = static_cast<float>(scroll_accumulator[0]);
        g_ctx.mouse.scroll[1] = static_cast<float>(scroll_accumulator[1]);

        if (g_ctx.reset_back_buffer)
        {
            g_ctx.reset_back_buffer = false;

            g_ctx.window.update_size_info();

            const uint16_t width  = static_cast<uint16_t>(g_ctx.window.framebuffer_width );
            const uint16_t height = static_cast<uint16_t>(g_ctx.window.framebuffer_height);

            const uint32_t vsync  = g_ctx.vsync_on ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

            bgfx::reset(width, height, BGFX_RESET_NONE | vsync);

            g_ctx.pass_cache.notify_backbuffer_size_changed();
        }

        if (update_cursor_position)
        {
            g_ctx.mouse.update_position(g_ctx.window);
        }

        g_ctx.mouse.update_position_delta();

        if (key_down(KEY_F12))
        {
            debug_state = debug_state ? BGFX_DEBUG_NONE : BGFX_DEBUG_STATS;
            bgfx::setDebug(debug_state);
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (draw)
        {
            (*draw)();
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (t_ctx->is_main_thread)
        {
            if (!t_ctx->encoder)
            {
                t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
                ASSERT(t_ctx->encoder);
            }

            // TODO : I guess ideally we touch all active passes in all local context (?).
            g_ctx.pass_cache[t_ctx->active_pass].touch();
            g_ctx.pass_cache.update(t_ctx->encoder);
        }

        for (LocalContext& ctx : t_ctxs)
        {
            if (ctx.encoder)
            {
                bgfx::end(ctx.encoder);
                ctx.encoder = nullptr;
            }
        }

        if (t_ctx->is_main_thread)
        {
            g_ctx.mesh_cache.clear_transient_meshes();
        }

        g_ctx.bgfx_frame_number = bgfx::frame();
        g_ctx.frame_number++;
    }

    if (cleanup)
    {
        (*cleanup)();
    }

    g_ctx.task_scheduler.WaitforAllAndShutdown();

    g_ctx.layout_cache      .clear();
    g_ctx.texture_cache     .clear();
    g_ctx.framebuffer_cache .clear();
    g_ctx.program_cache     .clear();
    g_ctx.default_uniforms  .clear();
    g_ctx.uniform_cache     .clear();
    g_ctx.mesh_cache        .clear();
    g_ctx.memory_cache.clear();

    bgfx::shutdown();

    glfwDestroyCursor(g_ctx.cursors[CURSOR_ARROW    ]);
    glfwDestroyCursor(g_ctx.cursors[CURSOR_CROSSHAIR]);
    glfwDestroyCursor(g_ctx.cursors[CURSOR_H_RESIZE ]);
    glfwDestroyCursor(g_ctx.cursors[CURSOR_HAND     ]);
    glfwDestroyCursor(g_ctx.cursors[CURSOR_I_BEAM   ]);
    glfwDestroyCursor(g_ctx.cursors[CURSOR_V_RESIZE ]);

    glfwDestroyWindow(g_ctx.window.handle);
    glfwTerminate();

    MNM_TRACE("Finished.");

    return 0;
}


// -----------------------------------------------------------------------------

} // namespace mnm


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY FROM C
// -----------------------------------------------------------------------------

int mnm_run(void (* init)(void), void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    return mnm::run(init, setup, draw, cleanup);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - WINDOW
// -----------------------------------------------------------------------------

void size(int width, int height, int flags)
{
    ASSERT(mnm::t_ctx->is_main_thread);
    ASSERT(mnm::g_ctx.window.display_scale_x);
    ASSERT(mnm::g_ctx.window.display_scale_y);

    // TODO : Round instead?
    if (mnm::g_ctx.window.position_scale_x != 1.0f) { width  = static_cast<int>(width  * mnm::g_ctx.window.display_scale_x); }
    if (mnm::g_ctx.window.position_scale_y != 1.0f) { height = static_cast<int>(height * mnm::g_ctx.window.display_scale_y); }

    mnm::resize_window(mnm::g_ctx.window.handle, width, height, flags);
}

void title(const char* title)
{
    ASSERT(mnm::t_ctx->is_main_thread);

    glfwSetWindowTitle(mnm::g_ctx.window.handle, title);
}

void vsync(int vsync)
{
    ASSERT(mnm::t_ctx->is_main_thread);

    mnm::g_ctx.vsync_on          = static_cast<bool>(vsync);
    mnm::g_ctx.reset_back_buffer = true;
}

void quit(void)
{
    ASSERT(mnm::t_ctx->is_main_thread);

    glfwSetWindowShouldClose(mnm::g_ctx.window.handle, GLFW_TRUE);
}

float width(void)
{
    return mnm::g_ctx.window.dpi_invariant_width;
}

float height(void)
{
    return mnm::g_ctx.window.dpi_invariant_height;
}

float aspect(void)
{
    return static_cast<float>(mnm::g_ctx.window.framebuffer_width) / static_cast<float>(mnm::g_ctx.window.framebuffer_height);
}

float dpi(void)
{
    return mnm::g_ctx.window.display_scale_x;
}

int dpi_changed(void)
{
    return mnm::g_ctx.window.display_scale_x_changed || !mnm::g_ctx.frame_number;
}

int pixel_width(void)
{
    return mnm::g_ctx.window.framebuffer_width;
}

int pixel_height(void)
{
    return mnm::g_ctx.window.framebuffer_height;
}


// -----------------------------------------------------------------------------
/// PUBLIC API IMPLEMENTATION - CURSOR
// -----------------------------------------------------------------------------

void cursor(int type)
{
    using namespace mnm;

    ASSERT(t_ctx->is_main_thread);
    ASSERT(type >= CURSOR_ARROW && type <= CURSOR_LOCKED);

    if (type != g_ctx.active_cursor)
    {
        g_ctx.active_cursor = type;

        switch (type)
        {
        case CURSOR_HIDDEN:
            glfwSetInputMode(g_ctx.window.handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case CURSOR_LOCKED:
            glfwSetInputMode(g_ctx.window.handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        default:
            if (type >= CURSOR_ARROW && type <= CURSOR_LOCKED)
            {
                glfwSetInputMode(g_ctx.window.handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursor   (g_ctx.window.handle, g_ctx.cursors[type]);
            }
        }
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - INPUT
// -----------------------------------------------------------------------------

float mouse_x(void)
{
    return mnm::g_ctx.mouse.curr[0];
}

float mouse_y(void)
{
    return mnm::g_ctx.mouse.curr[1];
}

float mouse_dx(void)
{
    return mnm::g_ctx.mouse.delta[0];
}

float mouse_dy(void)
{
    return mnm::g_ctx.mouse.delta[1];
}

int mouse_down(int button)
{
    return mnm::g_ctx.mouse.is(button, mnm::Mouse::DOWN);
}

int mouse_held(int button)
{
    return mnm::g_ctx.mouse.is(button, mnm::Mouse::HELD);
}

int mouse_up(int button)
{
    return mnm::g_ctx.mouse.is(button, mnm::Mouse::UP);
}

int mouse_clicked(int button)
{
    return mnm::g_ctx.mouse.repeated_click_count(button);
}

float mouse_held_time(int button)
{
    return mnm::g_ctx.mouse.held_time(button, static_cast<float>(mnm::g_ctx.total_time.elapsed));
}

float scroll_x(void)
{
    return mnm::g_ctx.mouse.scroll[0];
}

float scroll_y(void)
{
    return mnm::g_ctx.mouse.scroll[1];
}

int key_down(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::DOWN);
}

int key_repeated(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::REPEATED);
}

int key_held(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::HELD);
}

int key_up(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::UP);
}

float key_held_time(int key)
{
    return mnm::g_ctx.keyboard.held_time(key, static_cast<float>(mnm::g_ctx.total_time.elapsed));
}

unsigned int codepoint(void)
{
    using namespace mnm;

    unsigned int value = 0;

    // TODO : Make the queue thread safe.
    if (!g_ctx.codepoint_queue.empty())
    {
        value = g_ctx.codepoint_queue.back();
        g_ctx.codepoint_queue.pop_back();
    }

    return value;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TIME
// -----------------------------------------------------------------------------

double elapsed(void)
{
    return mnm::g_ctx.total_time.elapsed;
}

double dt(void)
{
    return mnm::g_ctx.frame_time.elapsed;
}

void sleep_for(double seconds)
{
    ASSERT(!mnm::t_ctx->is_main_thread);

    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

void tic(void)
{
    mnm::t_ctx->stop_watch.tic();
}

double toc(void)
{
    return mnm::t_ctx->stop_watch.toc();
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - GEOMETRY
// -----------------------------------------------------------------------------

void begin_mesh(int id, int flags)
{
    ASSERT(!mnm::t_ctx->mesh_recorder.is_recording());

    mnm::t_ctx->mesh_recorder.begin(
        static_cast<uint16_t>(id),
        static_cast<uint16_t>(flags) // NOTE : User exposed flags fit within 16 bits.
    );
}

void end_mesh(void)
{
    using namespace mnm;

    ASSERT(t_ctx->mesh_recorder.is_recording());

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.mesh_cache.add_mesh(t_ctx->mesh_recorder, g_ctx.layout_cache);

    t_ctx->mesh_recorder.end();
}

void vertex(float x, float y, float z)
{
    using namespace mnm;

    // TODO : We should measure whether branch prediction minimizes the cost of
    //        having a condition in here.
    if (!(t_ctx->mesh_recorder.flags() & NO_VERTEX_TRANSFORM))
    {
        t_ctx->mesh_recorder.vertex((t_ctx->matrix_stack.top() * HMM_Vec4(x, y, z, 1.0f)).XYZ);
    }
    else
    {
        t_ctx->mesh_recorder.vertex(HMM_Vec3(x, y, z));
    }
}

void color(unsigned int rgba)
{
    mnm::t_ctx->mesh_recorder.color(rgba);
}

void normal(float nx, float ny, float nz)
{
    mnm::t_ctx->mesh_recorder.normal(nx, ny, nz);
}

void texcoord(float u, float v)
{
    mnm::t_ctx->mesh_recorder.texcoord(u, v);
}

void mesh(int id)
{
    using namespace mnm;

    ASSERT(id > 0 && static_cast<uint16_t>(id) < MAX_MESHES);
    ASSERT(!t_ctx->mesh_recorder.is_recording());

    DrawState& state = t_ctx->draw_state;

    state.pass = t_ctx->active_pass;

    state.framebuffer = g_ctx.pass_cache[t_ctx->active_pass].framebuffer();

    const Mesh& mesh       = g_ctx.mesh_cache[static_cast<uint16_t>(id)];
    uint32_t    mesh_flags = mesh.flags;

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder);
    }

    if (bgfx::isValid(state.vertex_alias))
    {
        state.vertex_alias = g_ctx.layout_cache.resolve_alias(mesh_flags, state.vertex_alias.idx);
    }

    // TODO : Check whether instancing works together with the aliasing.
    if (state.instances)
    {
        t_ctx->encoder->setInstanceDataBuffer(&state.instances->buffer);

        if (state.instances->is_transform)
        {
            mesh_flags |= INSTANCING_SUPPORTED;
        }
    }

    if (mesh_flags & TEXT_MESH)
    {
        if (!bgfx::isValid(state.texture))
        {
            texture(mesh.extra_data);
        }

        if (state.flags == STATE_DEFAULT)
        {
            // NOTE : Maybe we just want ensure the blending is added?
            state.flags = STATE_BLEND_ALPHA | STATE_WRITE_RGB;
        }
    }

    if (!bgfx::isValid(state.program))
    {
        // TODO : Figure out how to do this without this ugliness.
        if (state.sampler.idx == g_ctx.default_uniforms.color_texture_r.idx)
        {
            mesh_flags |= SAMPLER_COLOR_R;
        }

        state.program = g_ctx.program_cache.builtin(mesh_flags);
    }

    if (state.element_start != 0 || state.element_count != UINT32_MAX)
    {
        // TODO : Emit warning if mesh has `OPTIMIZE_GEOMETRY` on.

        if (mesh_flags & PRIMITIVE_QUADS)
        {
            ASSERT(state.element_start % 4 == 0);
            ASSERT(state.element_count % 4 == 0);

            state.element_start = (state.element_start >> 1) * 3;
            state.element_count = (state.element_count >> 1) * 3;
        }
    }

    submit_mesh(
        mesh,
        t_ctx->matrix_stack.top(),
        state,
        g_ctx.mesh_cache.transient_buffers(),
        g_ctx.default_uniforms,
        *t_ctx->encoder
    );

    state = {};
}

void alias(int flags)
{
    mnm::t_ctx->draw_state.vertex_alias = { static_cast<uint16_t>(flags) };
}

void range(int start, int count)
{
    ASSERT(start >= 0);

    mnm::t_ctx->draw_state.element_start =              static_cast<uint32_t>(start) ;
    mnm::t_ctx->draw_state.element_count = count >= 0 ? static_cast<uint32_t>(count) : UINT32_MAX;
}

void state(int flags)
{
    mnm::t_ctx->draw_state.flags = static_cast<uint16_t>(flags);
}

void scissor(int x, int y, int width, int height)
{
    using namespace mnm;

    ASSERT(x >= 0);
    ASSERT(y >= 0);
    ASSERT(width >= 0);
    ASSERT(height >= 0);

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder);
    }

    t_ctx->encoder->setScissor(
        static_cast<uint16_t>(x),
        static_cast<uint16_t>(y),
        static_cast<uint16_t>(width ),
        static_cast<uint16_t>(height)
    );
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURING
// -----------------------------------------------------------------------------

void load_texture(int id, int flags, int width, int height, int stride, const void* data)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_TEXTURES);
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT((width < SIZE_EQUAL && height < SIZE_EQUAL) || (width <= SIZE_DOUBLE && width == height));
    ASSERT(stride >= 0);

    mnm::g_ctx.texture_cache.add_texture(
        static_cast<uint16_t>(id),
        static_cast<uint16_t>(flags),
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        static_cast<uint16_t>(stride),
        data
    );
}

void create_texture(int id, int flags, int width, int height)
{
    load_texture(id, flags, width, height, 0, nullptr);
}

void texture(int id)
{
    using namespace mnm;

    ASSERT(id > 0 && static_cast<uint16_t>(id) < MAX_TEXTURES);

    if (!t_ctx->framebuffer_recorder.is_recording())
    {
        // TODO : Samplers should be set by default state and only overwritten when
        //        non-default shader is used.
        const Texture& texture = g_ctx.texture_cache[static_cast<uint16_t>(id)];

        t_ctx->draw_state.texture         = texture.handle;
        t_ctx->draw_state.sampler         = g_ctx.default_uniforms.default_sampler(texture.format);
        t_ctx->draw_state.texture_size[0] = texture.width;
        t_ctx->draw_state.texture_size[1] = texture.height;
    }
    else
    {
        t_ctx->framebuffer_recorder.add_texture(g_ctx.texture_cache[static_cast<uint16_t>(id)]);
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURE READBACK
// -----------------------------------------------------------------------------

void read_texture(int id, void* data)
{
    using namespace mnm;

    ASSERT(id > 0 && static_cast<uint16_t>(id) < MAX_TEXTURES);

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder);
    }

    g_ctx.texture_cache.schedule_read(
        static_cast<uint16_t>(id),
        t_ctx->active_pass + MAX_PASSES, // TODO : It might be better to let the user specify the pass explicitly.
        t_ctx->encoder,
        data
    );
}

int readable(int id)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_TEXTURES);

    // TODO : This needs to compare value returned from `bgfx::frame`.
    return mnm::g_ctx.bgfx_frame_number >= mnm::g_ctx.texture_cache[static_cast<uint16_t>(id)].read_frame;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - INSTANCING
// -----------------------------------------------------------------------------

void begin_instancing(int id, int type)
{
    using namespace mnm;

    ASSERT(id >= 0 && static_cast<uint16_t>(id) < MAX_INSTANCE_BUFFERS);
    ASSERT(type >= INSTANCE_TRANSFORM && type <= INSTANCE_DATA_112);

    ASSERT(!t_ctx->instance_recorder.is_recording());

    t_ctx->instance_recorder.begin(static_cast<uint16_t>(id), static_cast<uint16_t>(type));
}

void end_instancing(void)
{
    using namespace mnm;

    ASSERT(t_ctx->instance_recorder.is_recording());

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.instance_cache.add_buffer(t_ctx->instance_recorder);

    t_ctx->instance_recorder.end();
}

void instance(const void* data)
{
    using namespace mnm;

    ASSERT(t_ctx->instance_recorder.is_recording());
    ASSERT((data == nullptr) == (t_ctx->instance_recorder.is_transform()));

    t_ctx->instance_recorder.instance(t_ctx->instance_recorder.is_transform() ? &t_ctx->matrix_stack.top() : data);
}

void instances(int id)
{
    using namespace mnm;

    ASSERT(id >= 0 && static_cast<uint16_t>(id) < MAX_INSTANCE_BUFFERS);

    // TODO : Assert that instance ID is active in the cache in the current frame.
    t_ctx->draw_state.instances = &g_ctx.instance_cache[static_cast<uint16_t>(id)];
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FONT ATLASING
// -----------------------------------------------------------------------------

void create_font(int id, const void* data)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_FONTS);
    ASSERT(data);

    mnm::g_ctx.font_data_registry.add(static_cast<uint16_t>(id), data);
}

void begin_atlas(int id, int flags, int font, float size)
{
    using namespace mnm;

    // TODO : Check `flags`.
    ASSERT(id > 0 && static_cast<uint16_t>(id) < MAX_TEXTURES);
    ASSERT(font > 0 && static_cast<uint16_t>(font) < MAX_FONTS);
    ASSERT(size >= 5.0f && size <= 4096.0f);

    // TODO : Signal error if can't get an atlas.
    if ((t_ctx->active_atlas = g_ctx.atlas_cache.get_or_create(static_cast<uint16_t>(id))))
    {
        t_ctx->active_atlas->reset(
            static_cast<uint16_t>(id),
            static_cast<uint16_t>(flags),
            g_ctx.font_data_registry[font],
            size,
            g_ctx.texture_cache
        );
    }
}

void end_atlas(void)
{
    using namespace mnm;

    ASSERT(t_ctx->active_atlas);

    t_ctx->active_atlas->update(g_ctx.texture_cache);
    t_ctx->active_atlas = nullptr;
}

void glyph_range(int first, int last)
{
    ASSERT(first >= 0);
    ASSERT(first <= last);
    ASSERT(last  <= UINT16_MAX);
    ASSERT(mnm::t_ctx->active_atlas);

    mnm::t_ctx->active_atlas->add_glyph_range(static_cast<uint32_t>(first), static_cast<uint32_t>(last));
}

void glyphs_from_string(const char* string)
{
    ASSERT(string);
    ASSERT(mnm::t_ctx->active_atlas);

    mnm::t_ctx->active_atlas->add_glyphs_from_string(string, nullptr);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXT MESHES
// -----------------------------------------------------------------------------

void begin_text(int id, int atlas, int flags)
{
    using namespace mnm;

    ASSERT(!t_ctx->text_recorder.mesh_recorder());

    t_ctx->text_recorder.begin(
        static_cast<uint16_t>(id),
        static_cast<uint16_t>(flags),
        static_cast<uint16_t>(atlas),
        g_ctx.atlas_cache.get(static_cast<uint16_t>(atlas)),
        &t_ctx->mesh_recorder
    );
}

void end_text(void)
{
    using namespace mnm;

    ASSERT(t_ctx->text_recorder.mesh_recorder());

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.mesh_cache.add_mesh(*t_ctx->text_recorder.mesh_recorder(), g_ctx.layout_cache);

    t_ctx->text_recorder.end();
}

void alignment(int flags)
{
    ASSERT(mnm::t_ctx->text_recorder.mesh_recorder());

    mnm::t_ctx->text_recorder.set_alignment(static_cast<uint16_t>(flags));
}

void line_height(float factor)
{
    ASSERT(mnm::t_ctx->text_recorder.mesh_recorder());

    mnm::t_ctx->text_recorder.set_line_height(factor);
}

void text(const char* start, const char* end)
{
    using namespace mnm;

    ASSERT(start);
    ASSERT(t_ctx->text_recorder.mesh_recorder());

    t_ctx->text_recorder.add_text(start, end, t_ctx->matrix_stack.top(), g_ctx.texture_cache);
}

void text_size(int atlas, const char* start, const char* end, float line_height, float* width, float* height)
{
    using namespace mnm;

    if (BX_UNLIKELY(!(width || height)))
    {
        return;
    }

    if (width ) { *width  = 0.0f; }
    if (height) { *height = 0.0f; }

    // TODO : Add warning if the ID doesn't correspond to a valid atlas.
    if (Atlas* ptr = g_ctx.atlas_cache.get(static_cast<uint16_t>(atlas)))
    {
        if (!ptr->get_text_size(start, end, line_height, width, height) &&
             ptr->is_updatable())
        {
            ptr->add_glyphs_from_string(start, end);
            ptr->update(g_ctx.texture_cache);

            const bool success = ptr->get_text_size(start, end, line_height, width, height);
            BX_UNUSED (success);
            ASSERT    (success);
        }
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - PASSES
// -----------------------------------------------------------------------------

void pass(int id)
{
    ASSERT(id >= 0 && static_cast<uint16_t>(id) < mnm::MAX_PASSES);

    mnm::t_ctx->active_pass = static_cast<uint16_t>(id);
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].touch();
}

void no_clear(void)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_no_clear();
}

void clear_depth(float depth)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_clear_depth(depth);
}

void clear_color(unsigned int rgba)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_clear_color(rgba);
}

void no_framebuffer(void)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_framebuffer(BGFX_INVALID_HANDLE);
}

void framebuffer(int id)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_FRAMEBUFFERS);

    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_framebuffer(
        mnm::g_ctx.framebuffer_cache[static_cast<uint16_t>(id)].handle
    );
}

void viewport(int x, int y, int width, int height)
{
    ASSERT(x      >= 0);
    ASSERT(y      >= 0);
    ASSERT(width  >  0);
    ASSERT(height >  0);

    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_viewport(
        static_cast<uint16_t>(x),
        static_cast<uint16_t>(y),
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height)
    );
}

void full_viewport(void)
{
    viewport(0, 0, SIZE_EQUAL, SIZE_EQUAL);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FRAMEBUFFERS
// -----------------------------------------------------------------------------

void begin_framebuffer(int id)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_FRAMEBUFFERS);

    mnm::t_ctx->framebuffer_recorder.begin(static_cast<uint16_t>(id));
}

void end_framebuffer(void)
{
    mnm::g_ctx.framebuffer_cache.add_framebuffer(mnm::t_ctx->framebuffer_recorder);
    mnm::t_ctx->framebuffer_recorder.end();
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - SHADERS
// -----------------------------------------------------------------------------

void create_uniform(int id, int type, int count, const char* name)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_UNIFORMS);
    ASSERT(type > 0 && type <= UNIFORM_SAMPLER);
    ASSERT(count > 0);
    ASSERT(name);

    (void)mnm::g_ctx.uniform_cache.add(
        static_cast<uint16_t>(id),
        static_cast<uint16_t>(type),
        static_cast<uint16_t>(count),
        name
    );
}

void uniform(int id, const void* value)
{
    using namespace mnm;

    ASSERT(id > 0 && static_cast<uint16_t>(id) < MAX_UNIFORMS);
    ASSERT(value);

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder);
    }

    t_ctx->encoder->setUniform(g_ctx.uniform_cache[static_cast<uint16_t>(id)].handle, value, UINT16_MAX);
}

void create_shader(int id, const void* vs_data, int vs_size, const void* fs_data, int fs_size)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_PROGRAMS);
    ASSERT(vs_data);
    ASSERT(vs_size > 0);
    ASSERT(fs_data);
    ASSERT(fs_size > 0);

    (void)mnm::g_ctx.program_cache.add(
        static_cast<uint16_t>(id),
        vs_data,
        static_cast<uint32_t>(vs_size),
        fs_data,
        static_cast<uint32_t>(fs_size)
    );
}

void shader(int id)
{
    ASSERT(id > 0 && static_cast<uint16_t>(id) < mnm::MAX_PROGRAMS);

    mnm::t_ctx->draw_state.program = mnm::g_ctx.program_cache[static_cast<uint16_t>(id)];
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TRANSFORMATIONS
// -----------------------------------------------------------------------------

void view(void)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_view(mnm::t_ctx->matrix_stack.top());
}

void projection(void)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_projection(mnm::t_ctx->matrix_stack.top());
}

void push(void)
{
    mnm::t_ctx->matrix_stack.push();
}

void pop(void)
{
    mnm::t_ctx->matrix_stack.pop();
}

void identity(void)
{
    mnm::t_ctx->matrix_stack.top() = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near_, float far_)
{
    mnm::t_ctx->matrix_stack.multiply_top(HMM_Orthographic(left, right, bottom, top, near_, far_));
}

void perspective(float fovy, float aspect, float near_, float far_)
{
    mnm::t_ctx->matrix_stack.multiply_top(HMM_Perspective(fovy, aspect, near_, far_));
}

void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y, float at_z, float up_x, float up_y, float up_z)
{
    mnm::t_ctx->matrix_stack.multiply_top(HMM_LookAt(HMM_Vec3(eye_x, eye_y, eye_z), HMM_Vec3(at_x, at_y, at_z), HMM_Vec3(up_x, up_y, up_z)));
}

void rotate(float angle, float x, float y, float z)
{
    mnm::t_ctx->matrix_stack.multiply_top(HMM_Rotate(angle, HMM_Vec3(x, y, z)));
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
    mnm::t_ctx->matrix_stack.multiply_top(HMM_Scale(HMM_Vec3(scale, scale, scale)));
}

void translate(float x, float y, float z)
{
    mnm::t_ctx->matrix_stack.multiply_top(HMM_Translate(HMM_Vec3(x, y, z)));
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MULTITHREADING
// -----------------------------------------------------------------------------

int task(void (* func)(void* data), void* data)
{
    mnm::Task* task = mnm::g_ctx.task_pool.get_free_task();

    if (task)
    {
        task->func = func;
        task->data = data;

        mnm::g_ctx.task_scheduler.AddTaskSetToPipe(task);
    }

    return task != nullptr;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - IMAGE IO
// -----------------------------------------------------------------------------

unsigned char* load_image(const char* file_name, int channels, int* width, int* height)
{
    ASSERT(file_name);
    ASSERT(channels >= 1 && channels <= 4);

    return mnm::g_ctx.memory_cache.load_image(file_name, channels, width, height);
}

unsigned char* decode_image(const void* data, int bytes, int channels, int* width, int* height)
{
    ASSERT(data);
    ASSERT(bytes > 0);
    ASSERT(channels >= 1 && channels <= 4);

    return mnm::g_ctx.memory_cache.decode_image(static_cast<const unsigned char*>(data), bytes, channels, width, height);
}

void save_image(const char* file_name, const void* data, int width, int height, int channels)
{
    ASSERT(file_name);
    ASSERT(data);
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT(channels >= 1 && channels <= 4);

    int success = 0;

    if (const char* ext = strrchr(file_name, '.'))
    {
        if (!strcasecmp(ext, ".png"))
        {
            success = stbi_write_png(file_name, width, height, channels, data, width * channels);
        }
        else if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg"))
        {
            success = stbi_write_jpg(file_name, width, height, channels, data, 85); // TODO : Expose the quality.
        }
        else if (!strcasecmp(ext, ".bmp"))
        {
            success = stbi_write_bmp(file_name, width, height, channels, data);
        }
        else if (!strcasecmp(ext, ".tga"))
        {
            success = stbi_write_tga(file_name, width, height, channels, data);
        }
    }

    ASSERT(success); // TODO : Report status.
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FILE IO
// -----------------------------------------------------------------------------

unsigned char* load_bytes(const char* file_name, int* bytes_read)
{
    ASSERT(file_name);

    return mnm::g_ctx.memory_cache.load_bytes(file_name, bytes_read);
}

void save_bytes(const char* file_name, const void* data, int bytes)
{
    ASSERT(file_name);
    ASSERT(data);
    ASSERT(bytes > 0);

    if (FILE* f = fopen(file_name, "wb"))
    {
        (void)fwrite(data, static_cast<size_t>(bytes), 1, f); // TODO : Check return value and report error.
        fclose(f);
    }
}

char* load_string(const char* file_name)
{
    ASSERT(file_name);

    return mnm::g_ctx.memory_cache.load_string(file_name);
}

void save_string(const char* file_name, const char* string)
{
    ASSERT(file_name);
    ASSERT(string);

    save_bytes(file_name, string, static_cast<int>(strlen(string)));
}

void unload(void* file_content)
{
    mnm::g_ctx.memory_cache.unload(file_content);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - PLATFORM INFO
// -----------------------------------------------------------------------------

int platform(void)
{
#if BX_PLATFORM_LINUX
    return PLATFORM_LINUX;
#elif BX_PLATFORM_OSX
    return PLATFORM_MACOS;
#elif BX_PLATFORM_WINDOWS
    return PLATFORM_WINDOWS;
#else
    return PLATFORM_UNKNOWN;
#endif
}

int renderer(void)
{
    switch (bgfx::getRendererType())
    {
    case bgfx::RendererType::Direct3D11:
        return RENDERER_DIRECT3D11;
    case bgfx::RendererType::Metal:
        return RENDERER_METAL;
    case bgfx::RendererType::OpenGL:
        return RENDERER_OPENGL;
    default:
        return RENDERER_UNKNOWN;
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MISCELLANEOUS
// -----------------------------------------------------------------------------

void transient_memory(int megabytes)
{
    ASSERT(megabytes > 0);

    mnm::g_ctx.transient_memory = megabytes << 20;
}

int frame(void)
{
    return static_cast<int>(mnm::g_ctx.frame_number);
}


// -----------------------------------------------------------------------------
// INTERNAL API
// -----------------------------------------------------------------------------

extern "C" GLFWwindow* mnm_get_window(void)
{
    return mnm::g_ctx.window.handle;
}
