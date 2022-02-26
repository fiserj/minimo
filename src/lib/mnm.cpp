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
#include <bx/mutex.h>
#include <bx/pixelformat.h>       // packRg16S, packRgb8
#include <bx/ringbuffer.h>        // RingBufferControl
#include <bx/sort.h>              // quickSort
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

#include "mnm_base.h"
#include "mnm_consts.h"
#include "mnm_array.h"
#include "mnm_input.h"
#include "mnm_window.h"
#include "mnm_stack.h"
#include "mnm_vertex_attribs.h"
#include "mnm_vertex_submission.h"
#include "mnm_vertex_layout.h"
#include "mnm_mesh_recorder.h"
#include "mnm_mesh_cache.h"
#include "mnm_texture_cache.h"
#include "mnm_utf8.h"
#include "mnm_font_atlas.h"
#include "mnm_instancing.h"

namespace mnm
{

// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T, u32 Size>
using Array = std::array<T, Size>;

template <typename T>
using Vector = std::vector<T>;


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

template <size_t Size>
static inline void push_back(Vector<u8>& buffer, const void* data)
{
    static_assert(Size > 0, "Size must be positive.");

    buffer.resize(buffer.size() + Size);

    assign<Size>(data, buffer.data() + buffer.size() - Size);
}

template <typename T>
static inline void push_back(Vector<u8>& buffer, const T& value)
{
    push_back<sizeof(T)>(buffer, &value);
}


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

    bool add(u16 id, u16 type, u16 count, const char* name)
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

    inline Uniform& operator[](u16 id) { return m_uniforms[id]; }

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
    u32                 element_start   = 0;
    u32                 element_count   = UINT32_MAX;
    bgfx::ViewId             pass            = UINT16_MAX;
    bgfx::FrameBufferHandle  framebuffer     = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      program         = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle      texture         = BGFX_INVALID_HANDLE; // TODO : More texture slots.
    bgfx::UniformHandle      sampler         = BGFX_INVALID_HANDLE;
    u16                 texture_size[2] = { 0, 0 };
    bgfx::VertexLayoutHandle vertex_alias    = BGFX_INVALID_HANDLE;
    u16                 flags           = STATE_DEFAULT;
    u8                  _pad[10];
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

    bool add(u16 id, bgfx::ShaderHandle vertex, bgfx::ShaderHandle fragment, u32 attribs = UINT32_MAX)
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

    bool add(u16 id, const bgfx::EmbeddedShader* shaders, bgfx::RendererType::Enum renderer, const char* vertex_name, const char* fragment_name, u32 attribs = UINT32_MAX)
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

    bool add(u16 id, const void* vertex_data, u32 vertex_size, const void* fragment_data, u32 fragment_size, u32 attribs = UINT32_MAX)
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

    inline bgfx::ProgramHandle operator[](u16 id) const
    {
        return m_handles[id];
    }

    inline bgfx::ProgramHandle builtin(u32 attribs) const
    {
        auto idx = get_index_from_attribs(attribs);
        return m_builtins[get_index_from_attribs(attribs)];
    }

private:
    static inline constexpr u16 get_index_from_attribs(u32 attribs)
    {
        static_assert(
            VERTEX_ATTRIB_MASK   >> VERTEX_ATTRIB_SHIFT == 0b000111 &&
            INSTANCING_SUPPORTED >> 17                  == 0b001000 &&
            SAMPLER_COLOR_R      >> 17                  == 0b010000 &&
            VERTEX_PIXCOORD      >> 18                  == 0b100000,
            "Invalid index assumptions in `ProgramCache::get_index_from_attribs`."
        );

        return
            ((attribs & VERTEX_ATTRIB_MASK  ) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
            ((attribs & INSTANCING_SUPPORTED) >> 17                 ) | // Bit 3.
            ((attribs & SAMPLER_COLOR_R     ) >> 17                 ) | // Bit 4.
            ((attribs & VERTEX_PIXCOORD     ) >> 18                 ) ; // Bit 5.
    }

private:
    static constexpr u32                MAX_BUILTINS = 64;

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
                bgfx::setViewRect(id, m_viewport_x, m_viewport_y, bgfx::BackbufferRatio::Enum(m_viewport_width - SIZE_EQUAL));
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

    void set_clear_depth(f32 depth)
    {
        if (m_clear_depth != depth || !(m_dirty_flags & BGFX_CLEAR_DEPTH) || !(m_clear_flags & BGFX_CLEAR_DEPTH))
        {
            m_clear_flags |= BGFX_CLEAR_DEPTH;
            m_clear_depth  = depth;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    void set_clear_color(u32 rgba)
    {
        if (m_clear_rgba != rgba || !(m_dirty_flags & BGFX_CLEAR_COLOR) || !(m_clear_flags & BGFX_CLEAR_COLOR))
        {
            m_clear_flags |= BGFX_CLEAR_COLOR;
            m_clear_rgba   = rgba;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    inline void set_viewport(u16 x, u16 y, u16 width, u16 height)
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

    inline bgfx::FrameBufferHandle framebuffer() const
    {
        return m_framebuffer;
    }

private:
    enum : u8
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

    u16                     m_viewport_x      = 0;
    u16                     m_viewport_y      = 0;
    u16                     m_viewport_width  = SIZE_EQUAL;
    u16                     m_viewport_height = SIZE_EQUAL;

    bgfx::FrameBufferHandle m_framebuffer     = BGFX_INVALID_HANDLE;

    u16                     m_clear_flags     = BGFX_CLEAR_NONE;
    f32                     m_clear_depth     = 1.0f;
    u32                     m_clear_rgba      = 0x000000ff;
    u8                      m_clear_stencil   = 0;

    u8                      m_dirty_flags     = DIRTY_CLEAR;
};

struct PassCache
{
    Array<Pass, MAX_PASSES> passes;
    bool                    backbuffer_size_changed = true;

    void update(bgfx::Encoder* encoder)
    {
        for (bgfx::ViewId id = 0; id < passes.size(); id++)
        {
            passes[id].update(id, encoder, backbuffer_size_changed);
        }

        backbuffer_size_changed = false;
    }

    inline Pass& operator[](bgfx::ViewId i)
    {
        return passes[i];
    }
};


// -----------------------------------------------------------------------------
// INSTANCE CACHE
// -----------------------------------------------------------------------------

struct InstanceData
{
    bgfx::InstanceDataBuffer buffer       = { nullptr, 0, 0, 0, 0, BGFX_INVALID_HANDLE };
    bool                     is_transform = false;
    u8                       _pad[7];
};

struct InstanceCache
{
    Mutex                                     mutex;
    Array<InstanceData, MAX_INSTANCE_BUFFERS> data;

    bool add_buffer(const RecordInfo& info, const InstanceRecorder& recorder)
    {
        ASSERT(info.id < data.size());

        MutexScope lock(mutex);

        const u32 count     = recorder.instance_count();
        const u16 stride    = recorder.instance_size;
        const u32 available = bgfx::getAvailInstanceDataBuffer(count, stride);

        if (available < count)
        {
            // TODO : Handle this, inform user.
            return false;
        }

        InstanceData &instance_data = data[info.id];
        instance_data.is_transform = info.is_transform;
        bgfx::allocInstanceDataBuffer(&instance_data.buffer, count, stride);
        bx::memCopy(instance_data.buffer.data, recorder.buffer.data, recorder.buffer.size);

        return true;
    }

    inline const InstanceData& operator[](u16 id) const
    {
        return data[id];
    }
};


// -----------------------------------------------------------------------------
// GEOMETRY SUBMISSION
// -----------------------------------------------------------------------------

static inline u64 translate_draw_state_flags(u16 flags)
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

    constexpr u32 BLEND_STATE_MASK       = STATE_BLEND_ADD | STATE_BLEND_ALPHA | STATE_BLEND_MAX | STATE_BLEND_MIN;
    constexpr u32 BLEND_STATE_SHIFT      = 0;

    constexpr u32 CULL_STATE_MASK        = STATE_CULL_CCW | STATE_CULL_CW;
    constexpr u32 CULL_STATE_SHIFT       = 4;

    constexpr u32 DEPTH_TEST_STATE_MASK  = STATE_DEPTH_TEST_GEQUAL | STATE_DEPTH_TEST_GREATER | STATE_DEPTH_TEST_LEQUAL | STATE_DEPTH_TEST_LESS;
    constexpr u32 DEPTH_TEST_STATE_SHIFT = 6;

    constexpr u64 blend_table[] =
    {
        0,
        BGFX_STATE_BLEND_ADD,
        BGFX_STATE_BLEND_ALPHA,
        BGFX_STATE_BLEND_LIGHTEN,
        BGFX_STATE_BLEND_DARKEN,
    };

    constexpr u64 cull_table[] =
    {
        0,
        BGFX_STATE_CULL_CCW,
        BGFX_STATE_CULL_CW,
    };

    constexpr u64 depth_test_table[] =
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
    const DynamicArray<bgfx::TransientVertexBuffer>& transient_buffers,
    const DefaultUniforms&                     default_uniforms,
    bgfx::Encoder&                             encoder
)
{
    static const u64 primitive_flags[] =
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
        const f32 data[] =
        {
            f32(state.texture_size[0]),
            f32(state.texture_size[1]),
            f32(state.texture_size[0]) ? 1.0f / f32(state.texture_size[0]) : 0.0f,
            f32(state.texture_size[1]) ? 1.0f / f32(state.texture_size[1]) : 0.0f
        };

        encoder.setUniform(default_uniforms.texture_size, data);
    }

    encoder.setTransform(&transform);

    u64 flags = translate_draw_state_flags(state.flags);

    flags |= primitive_flags[(mesh.flags & PRIMITIVE_TYPE_MASK) >> PRIMITIVE_TYPE_SHIFT];

    encoder.setState(flags);

    ASSERT(bgfx::isValid(state.program));
    encoder.submit(state.pass, state.program);
}


// -----------------------------------------------------------------------------
// FRAMEBUFFERS
// -----------------------------------------------------------------------------

struct Framebuffer
{
    bgfx::FrameBufferHandle handle = BGFX_INVALID_HANDLE;
    u16                     width  = 0;
    u16                     height = 0;

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
    inline void begin(u16 id)
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
            framebuffer.handle = bgfx::createFrameBuffer(u8(m_textures.size()),
                m_textures.data(), false);
            ASSERT(bgfx::isValid(framebuffer.handle));

            framebuffer.width  = m_width;
            framebuffer.height = m_height;
        }

        return framebuffer;
    }

    inline bool is_recording() const { return m_id != UINT16_MAX; }

    inline u16 id() const { return m_id; }

private:
    Vector<bgfx::TextureHandle> m_textures;
    u16                    m_id     = UINT16_MAX;
    u16                    m_width  = 0;
    u16                    m_height = 0;
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

    inline const Framebuffer& operator[](u16 id) const
    {
        return m_framebuffers[id];
    }

private:
    Mutex                                m_mutex;
    Array<Framebuffer, MAX_FRAMEBUFFERS> m_framebuffers;
};


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
    inline void add(u16 id, const void* data)
    {
        m_data[id] = data;
    }

    inline const void* operator[](u16 id) const
    {
        return m_data[id];
    }

private:
    Array<const void*, MAX_FONTS> m_data;
};

class AtlasCache
{
public:
    AtlasCache()
    {
        m_indices.fill(UINT16_MAX);
    }

    inline Atlas* get(u16 id)
    {
        return m_indices[id] != UINT16_MAX ? &m_atlases[m_indices[id]] : nullptr;
    }

    Atlas* get_or_create(u16 id)
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
                m_indices[id] = u16(i);

                return &m_atlases[i];
            }
        }

        ASSERT(false && "No more free atlas slots.");

        return nullptr;
    }

private:
    Mutex                             m_mutex;
    Array<Atlas, MAX_TEXTURE_ATLASES> m_atlases;
    StaticArray<u16, MAX_TEXTURES>    m_indices;
};


// -----------------------------------------------------------------------------
// TEXT MESH RECORDING
// -----------------------------------------------------------------------------

class TextRecorder
{
public:
    void reset(u16 flags, Atlas* atlas, MeshRecorder* recorder)
    {
        ASSERT(atlas);
        ASSERT(recorder);

        m_flags       = flags;
        m_atlas       = atlas;
        m_recorder    = recorder;
        m_line_height = 2.0f;
    }

    void clear()
    {
        *this = {};
    }

    void set_alignment(u16 flags)
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

    void set_line_height(f32 factor)
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
    inline u16 h_alignment() const
    {
        constexpr u16 alignment[] =
        {
            TEXT_H_ALIGN_LEFT  ,
            TEXT_H_ALIGN_CENTER,
            TEXT_H_ALIGN_RIGHT ,
        };

        return alignment[(m_flags & TEXT_H_ALIGN_MASK) >> TEXT_H_ALIGN_SHIFT];
    }

    inline u16 v_alignment() const
    {
        constexpr u16 alignment[] =
        {
            TEXT_V_ALIGN_BASELINE  ,
            TEXT_V_ALIGN_MIDDLE    ,
            TEXT_V_ALIGN_CAP_HEIGHT,
        };

        return alignment[(m_flags & TEXT_V_ALIGN_MASK) >> TEXT_V_ALIGN_SHIFT];
    }

    inline u16 y_axis() const
    {
        constexpr u16 alignment[] =
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
    f32         m_line_height = 2.0f;
    u16      m_flags       = 0;
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
// TASK POOL
// -----------------------------------------------------------------------------

class TaskPool;

struct Task : enki::ITaskSet
{
    void    (*func)(void*) = nullptr;
    void*     data         = nullptr;
    TaskPool* pool         = nullptr;

    void ExecuteRange(enki::TaskSetPartition, u32) override;
};

class TaskPool
{
public:
    TaskPool()
    {
        for (u8 i = 0; i < MAX_TASKS; i++)
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
            const u32 i = m_head;

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
        m_head          = u8(i);
    }

private:
    Mutex   m_mutex;
    Task    m_tasks[MAX_TASKS];
    u8 m_nexts[MAX_TASKS];
    u8 m_head = 0;

    static_assert(MAX_TASKS <= UINT8_MAX, "MAX_TASKS too big, change the type.");
};

void Task::ExecuteRange(enki::TaskSetPartition, u32)
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
        u8* image_data   = stbi_load(file_name, &image_width, &image_height, nullptr, channels);

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

        Vector<u8> buffer(image_data, image_data + image_width * image_height * channels);
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
        u8* image_data   = stbi_load_from_memory(data, bytes, &image_width, &image_height, nullptr, channels);

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

        Vector<u8> buffer(image_data, image_data + image_width * image_height * channels);
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

                Vector<u8> buffer(length + (type == STRING));

                if (fread(buffer.data(), 1, length, f) == size_t(length))
                {
                    if (type == STRING)
                    {
                        buffer.back() = 0;
                    }

                    content = buffer.data();

                    if (bytes_read)
                    {
                        *bytes_read = int(length);
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
    HashMap<void*, Vector<u8>> m_contents;
};


// -----------------------------------------------------------------------------
// CODEPINT QUEUE
// -----------------------------------------------------------------------------

template <u32 Capacity>
class CodepointQueue
{
    bx::RingBufferControl      m_ring;
    StaticArray<u32, Capacity> m_codepoints;

public:
    CodepointQueue()
        : m_ring(Capacity)
    {
    }

    void flush()
    {
        m_ring.reset();
    }

    void add(u32 codepoint)
    {
        while (!m_ring.reserve(1))
        {
            next();
        }

        m_codepoints[m_ring.m_current] = codepoint;
        m_ring.commit(1);
    }

    u32 next()
    {
        if (m_ring.available())
        {
            const u32 codepoint = m_codepoints[m_ring.m_read];
            m_ring.consume(1);

            return codepoint;
        }

        return 0;
    }
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
    KeyboardInput        keyboard;
    MouseInput           mouse;

    // VertexAttribStateFuncTable vertex_attrib_funcs = {};

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
    CodepointQueue<16>  codepoint_queue;

    GLFWcursor*         cursors[6] = { nullptr };
    Window              window;

    Timer               total_time;
    Timer               frame_time;

    int                 active_cursor     = 0;
    u32            frame_number      = 0;
    u32            bgfx_frame_number = 0;

    Atomic<int>         transient_memory  = 32 << 20;

    Atomic<bool>        vsync_on          = false;
    bool                reset_back_buffer = true;
};

struct LocalContext
{
    MeshRecorder        mesh_recorder;
    TextRecorder        text_recorder;
    InstanceRecorder    instance_recorder;
    RecordInfo          record_info;

    FramebufferRecorder framebuffer_recorder;

    DrawState           draw_state;

    MatrixStack<16>     matrix_stack;

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
        init.platformData           = create_platform_data (g_ctx.window.handle, init.type  );
        init.resolution.width       = u32(g_ctx.window.framebuffer_size[0]);
        init.resolution.height      = u32(g_ctx.window.framebuffer_size[1]);
        init.limits.transientVbSize = u32(g_ctx.transient_memory          );

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
            PinnedTask(u32 idx)
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

        for (u32 i = 0; i < g_ctx.task_scheduler.GetNumTaskThreads(); i++)
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
    // g_ctx.vertex_attrib_funcs.init();
    MeshRecorder::s_attrib_state_func_table.init();
    MeshRecorder::s_vertex_push_func_table.init();

    if (setup)
    {
        (*setup)();
    }

    g_ctx.bgfx_frame_number = bgfx::frame();

    u32 debug_state = BGFX_DEBUG_NONE;
    bgfx::setDebug(debug_state);

    const bgfx::RendererType::Enum    type        = bgfx::getRendererType();
    static const bgfx::EmbeddedShader s_shaders[] =
    {
        BGFX_EMBEDDED_SHADER(position_fs                 ),
        BGFX_EMBEDDED_SHADER(position_vs                 ),

        BGFX_EMBEDDED_SHADER(position_color_fs           ),
        BGFX_EMBEDDED_SHADER(position_color_vs           ),

        BGFX_EMBEDDED_SHADER(position_color_normal_fs    ),
        BGFX_EMBEDDED_SHADER(position_color_normal_vs    ),

        BGFX_EMBEDDED_SHADER(position_color_texcoord_fs  ),
        BGFX_EMBEDDED_SHADER(position_color_texcoord_vs  ),

        BGFX_EMBEDDED_SHADER(position_normal_fs          ),
        BGFX_EMBEDDED_SHADER(position_normal_vs          ),

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
            u32    attribs;
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
                VERTEX_COLOR | VERTEX_NORMAL,
                "position_color_normal"
            },
            {
                VERTEX_COLOR | VERTEX_TEXCOORD,
                "position_color_texcoord"
            },
            {
                VERTEX_NORMAL,
                "position_normal"
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

    g_ctx.mouse.update_position(
        g_ctx.window.handle,
        HMM_Vec2(g_ctx.window.position_scale[0], g_ctx.window.position_scale[1])
    );

    g_ctx.total_time.tic();
    g_ctx.frame_time.tic();

    g_ctx.frame_number = 0;

    while (!glfwWindowShouldClose(g_ctx.window.handle))
    {
        g_ctx.keyboard.update_states();
        g_ctx.mouse   .update_states();

        g_ctx.total_time.toc();
        g_ctx.frame_time.toc(true);

        g_ctx.codepoint_queue.flush();

        glfwPollEvents();

        bool   update_cursor_position = false;
        double scroll_accumulator[2]  = { 0.0, 0.0 }; // NOTE : Not sure if we can get multiple scroll events in a single frame.

        GLEQevent event;
        while (gleqNextEvent(&event))
        {
            switch (event.type)
            {
            case GLEQ_KEY_PRESSED:
                g_ctx.keyboard.update_input(event.keyboard.key, InputState::DOWN, f32(g_ctx.total_time.elapsed));
                break;

            case GLEQ_KEY_REPEATED:
                g_ctx.keyboard.update_input(event.keyboard.key, InputState::REPEATED);
                break;

            case GLEQ_KEY_RELEASED:
                g_ctx.keyboard.update_input(event.keyboard.key, InputState::UP);
                break;

            case GLEQ_BUTTON_PRESSED:
                g_ctx.mouse.update_input(event.mouse.button, InputState::DOWN, f32(g_ctx.total_time.elapsed));
                break;

            case GLEQ_BUTTON_RELEASED:
                g_ctx.mouse.update_input(event.mouse.button, InputState::UP);
                break;

            case GLEQ_CURSOR_MOVED:
                update_cursor_position = true;
                break;

            case GLEQ_SCROLLED:
                scroll_accumulator[0] += event.scroll.x;
                scroll_accumulator[1] += event.scroll.y;
                break;

            case GLEQ_CODEPOINT_INPUT:
                g_ctx.codepoint_queue.add(event.codepoint);
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

        g_ctx.mouse.scroll[0] = f32(scroll_accumulator[0]);
        g_ctx.mouse.scroll[1] = f32(scroll_accumulator[1]);

        if (g_ctx.reset_back_buffer)
        {
            g_ctx.reset_back_buffer = false;

            g_ctx.window.update_size_info();

            const u16 width  = u16(g_ctx.window.framebuffer_size[0] );
            const u16 height = u16(g_ctx.window.framebuffer_size[1]);

            const u32 vsync  = g_ctx.vsync_on ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

            bgfx::reset(width, height, BGFX_RESET_NONE | vsync);

            g_ctx.pass_cache.backbuffer_size_changed = true;
        }

        if (update_cursor_position)
        {
            g_ctx.mouse.update_position(
                g_ctx.window.handle,
                HMM_Vec2(g_ctx.window.position_scale[0], g_ctx.window.position_scale[1])
            );
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
// TEMPORARY TYPEDEFS IN GLOBAL NAMESPACE
// -----------------------------------------------------------------------------

using u8  = mnm::u8;
using u16 = mnm::u16;
using u32 = mnm::u32;
using u64 = mnm::u64;

using i8  = mnm::i8;
using i16 = mnm::i16;
using i32 = mnm::i32;
using i64 = mnm::i64;

using f32 = mnm::f32;
using f64 = mnm::f64;


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
    ASSERT(mnm::g_ctx.window.display_scale[0]);
    ASSERT(mnm::g_ctx.window.display_scale[1]);

    // TODO : Round instead?
    if (mnm::g_ctx.window.position_scale[0] != 1.0f) { width  = int(width  * mnm::g_ctx.window.display_scale[0]); }
    if (mnm::g_ctx.window.position_scale[1] != 1.0f) { height = int(height * mnm::g_ctx.window.display_scale[1]); }

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

    mnm::g_ctx.vsync_on          = bool(vsync);
    mnm::g_ctx.reset_back_buffer = true;
}

void quit(void)
{
    ASSERT(mnm::t_ctx->is_main_thread);

    glfwSetWindowShouldClose(mnm::g_ctx.window.handle, GLFW_TRUE);
}

float width(void)
{
    return mnm::g_ctx.window.invariant_size[0];
}

float height(void)
{
    return mnm::g_ctx.window.invariant_size[1];
}

float aspect(void)
{
    return f32(mnm::g_ctx.window.framebuffer_size[0]) / f32(mnm::g_ctx.window.framebuffer_size[1]);
}

float dpi(void)
{
    return mnm::g_ctx.window.display_scale[0];
}

int dpi_changed(void)
{
    return mnm::g_ctx.window.display_scale_changed || !mnm::g_ctx.frame_number;
}

int pixel_width(void)
{
    return mnm::g_ctx.window.framebuffer_size[0];
}

int pixel_height(void)
{
    return mnm::g_ctx.window.framebuffer_size[1];
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
    return mnm::g_ctx.mouse.current[0];
}

float mouse_y(void)
{
    return mnm::g_ctx.mouse.current[1];
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
    return mnm::g_ctx.mouse.is(button, mnm::InputState::DOWN);
}

int mouse_held(int button)
{
    return mnm::g_ctx.mouse.is(button, mnm::InputState::HELD);
}

int mouse_up(int button)
{
    return mnm::g_ctx.mouse.is(button, mnm::InputState::UP);
}

int mouse_clicked(int button)
{
    return mnm::g_ctx.mouse.repeated_click_count(button);
}

float mouse_held_time(int button)
{
    return mnm::g_ctx.mouse.held_time(button, f32(mnm::g_ctx.total_time.elapsed));
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
    return mnm::g_ctx.keyboard.is(key, mnm::InputState::DOWN);
}

int key_repeated(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::InputState::REPEATED);
}

int key_held(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::InputState::HELD);
}

int key_up(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::InputState::UP);
}

float key_held_time(int key)
{
    return mnm::g_ctx.keyboard.held_time(key, f32(mnm::g_ctx.total_time.elapsed));
}

unsigned int codepoint(void)
{
    return mnm::g_ctx.codepoint_queue.next();
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
    using namespace mnm;

    t_ctx->mesh_recorder.reset(u16(flags)); // NOTE : User exposed flags fit within 16 bits.

    t_ctx->record_info.flags      = u16(flags);
    t_ctx->record_info.extra_data = 0;
    t_ctx->record_info.id         = u16(id);
}

void end_mesh(void)
{
    using namespace mnm;

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.mesh_cache.add_mesh(
        t_ctx->record_info,
        t_ctx->mesh_recorder,
        g_ctx.layout_cache
    );

    t_ctx->mesh_recorder.clear();
}

void vertex(float x, float y, float z)
{
    using namespace mnm;

    // TODO : We should measure whether branch prediction minimizes the cost of
    //        having a condition in here.
    if (!(t_ctx->record_info.flags & NO_VERTEX_TRANSFORM))
    {
        t_ctx->mesh_recorder.vertex((t_ctx->matrix_stack.top * HMM_Vec4(x, y, z, 1.0f)).XYZ);
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

    ASSERT(id > 0 && u16(id) < MAX_MESHES);
    ASSERT(t_ctx->mesh_recorder.position_buffer.is_empty());

    DrawState& state = t_ctx->draw_state;

    state.pass = t_ctx->active_pass;

    state.framebuffer = g_ctx.pass_cache[t_ctx->active_pass].framebuffer();

    const Mesh& mesh       = g_ctx.mesh_cache[u16(id)];
    u32    mesh_flags = mesh.flags;

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
        ASSERT(bgfx::isValid(state.program));
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
        t_ctx->matrix_stack.top,
        state,
        g_ctx.mesh_cache.transient_buffers(),
        g_ctx.default_uniforms,
        *t_ctx->encoder
    );

    state = {};
}

void alias(int flags)
{
    mnm::t_ctx->draw_state.vertex_alias = { u16(flags) };
}

void range(int start, int count)
{
    ASSERT(start >= 0);

    mnm::t_ctx->draw_state.element_start =              u32(start) ;
    mnm::t_ctx->draw_state.element_count = count >= 0 ? u32(count) : UINT32_MAX;
}

void state(int flags)
{
    mnm::t_ctx->draw_state.flags = u16(flags);
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
        u16(x),
        u16(y),
        u16(width ),
        u16(height)
    );
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURING
// -----------------------------------------------------------------------------

void load_texture(int id, int flags, int width, int height, int stride, const void* data)
{
    ASSERT(id > 0 && u16(id) < mnm::MAX_TEXTURES);
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT((width < SIZE_EQUAL && height < SIZE_EQUAL) || (width <= SIZE_DOUBLE && width == height));
    ASSERT(stride >= 0);

    mnm::g_ctx.texture_cache.add_texture(
        u16(id),
        u16(flags),
        u16(width),
        u16(height),
        u16(stride),
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

    ASSERT(id > 0 && u16(id) < MAX_TEXTURES);

    if (!t_ctx->framebuffer_recorder.is_recording())
    {
        // TODO : Samplers should be set by default state and only overwritten when
        //        non-default shader is used.
        const Texture& texture = g_ctx.texture_cache[u16(id)];

        t_ctx->draw_state.texture         = texture.handle;
        t_ctx->draw_state.sampler         = g_ctx.default_uniforms.default_sampler(texture.format);
        t_ctx->draw_state.texture_size[0] = texture.width;
        t_ctx->draw_state.texture_size[1] = texture.height;
    }
    else
    {
        t_ctx->framebuffer_recorder.add_texture(g_ctx.texture_cache[u16(id)]);
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURE READBACK
// -----------------------------------------------------------------------------

void read_texture(int id, void* data)
{
    using namespace mnm;

    ASSERT(id > 0 && u16(id) < MAX_TEXTURES);

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder);
    }

    g_ctx.texture_cache.schedule_read(
        u16(id),
        t_ctx->active_pass + MAX_PASSES, // TODO : It might be better to let the user specify the pass explicitly.
        t_ctx->encoder,
        data
    );
}

int readable(int id)
{
    using namespace mnm;

    ASSERT(id > 0 && u16(id) < MAX_TEXTURES);

    // TODO : This needs to compare value returned from `bgfx::frame`.
    return g_ctx.bgfx_frame_number >= g_ctx.texture_cache[u16(id)].read_frame;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - INSTANCING
// -----------------------------------------------------------------------------

void begin_instancing(int id, int type)
{
    using namespace mnm;

    ASSERT(id >= 0 && u16(id) < MAX_INSTANCE_BUFFERS);
    ASSERT(type >= INSTANCE_TRANSFORM && type <= INSTANCE_DATA_112);

    t_ctx->instance_recorder.reset(u16(type));

    t_ctx->record_info.id           = u16(id);
    t_ctx->record_info.is_transform = type == INSTANCE_TRANSFORM;
}

void end_instancing(void)
{
    using namespace mnm;

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.instance_cache.add_buffer(
        t_ctx->record_info,
        t_ctx->instance_recorder
    );

    t_ctx->instance_recorder.clear();
}

void instance(const void* data)
{
    using namespace mnm;

    t_ctx->instance_recorder.instance(t_ctx->record_info.is_transform
        ? &t_ctx->matrix_stack.top : data
    );
}

void instances(int id)
{
    using namespace mnm;

    ASSERT(id >= 0 && u16(id) < MAX_INSTANCE_BUFFERS);

    // TODO : Assert that instance ID is active in the cache in the current frame.
    t_ctx->draw_state.instances = &g_ctx.instance_cache[u16(id)];
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FONT ATLASING
// -----------------------------------------------------------------------------

void create_font(int id, const void* data)
{
    ASSERT(id > 0 && u16(id) < mnm::MAX_FONTS);
    ASSERT(data);

    mnm::g_ctx.font_data_registry.add(u16(id), data);
}

void begin_atlas(int id, int flags, int font, float size)
{
    using namespace mnm;

    // TODO : Check `flags`.
    ASSERT(id > 0 && u16(id) < MAX_TEXTURES);
    ASSERT(font > 0 && u16(font) < MAX_FONTS);
    ASSERT(size >= 5.0f && size <= 4096.0f);

    // TODO : Signal error if can't get an atlas.
    if ((t_ctx->active_atlas = g_ctx.atlas_cache.get_or_create(u16(id))))
    {
        t_ctx->active_atlas->reset(
            u16(id),
            u16(flags),
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

    mnm::t_ctx->active_atlas->add_glyph_range(u32(first), u32(last));
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

void begin_text(int mesh_id, int atlas_id, int flags)
{
    using namespace mnm;

    ASSERT(!t_ctx->text_recorder.mesh_recorder());

    Atlas* atlas = g_ctx.atlas_cache.get(u16(atlas_id));
    ASSERT(atlas);

    const u32 mesh_flags =
        TEXT_MESH       |
        PRIMITIVE_QUADS |
        VERTEX_POSITION |
        VERTEX_TEXCOORD |
        VERTEX_COLOR    |
        (atlas->is_updatable() ? (TEXCOORD_F32 | VERTEX_PIXCOORD) : 0) |
        (flags & TEXT_TYPE_MASK);

    t_ctx->mesh_recorder.reset(mesh_flags);
    t_ctx->text_recorder.reset(u16(flags), atlas, &t_ctx->mesh_recorder);

    t_ctx->record_info.flags      = mesh_flags;
    t_ctx->record_info.extra_data = u16(atlas_id);
    t_ctx->record_info.id         = u16(mesh_id);
}

void end_text(void)
{
    using namespace mnm;

    ASSERT(t_ctx->text_recorder.mesh_recorder());

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.mesh_cache.add_mesh(
        t_ctx->record_info,
        t_ctx->mesh_recorder,
        g_ctx.layout_cache
    );

    t_ctx->text_recorder.clear();
    t_ctx->mesh_recorder.clear();
}

void alignment(int flags)
{
    ASSERT(mnm::t_ctx->text_recorder.mesh_recorder());

    mnm::t_ctx->text_recorder.set_alignment(u16(flags));
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

    t_ctx->text_recorder.add_text(start, end, t_ctx->matrix_stack.top, g_ctx.texture_cache);
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
    if (Atlas* ptr = g_ctx.atlas_cache.get(u16(atlas)))
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
    ASSERT(id >= 0 && u16(id) < mnm::MAX_PASSES);

    mnm::t_ctx->active_pass = u16(id);
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
    ASSERT(id > 0 && u16(id) < mnm::MAX_FRAMEBUFFERS);

    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_framebuffer(
        mnm::g_ctx.framebuffer_cache[u16(id)].handle
    );
}

void viewport(int x, int y, int width, int height)
{
    ASSERT(x      >= 0);
    ASSERT(y      >= 0);
    ASSERT(width  >  0);
    ASSERT(height >  0);

    mnm::g_ctx.pass_cache[mnm::t_ctx->active_pass].set_viewport(
        u16(x),
        u16(y),
        u16(width),
        u16(height)
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
    ASSERT(id > 0 && u16(id) < mnm::MAX_FRAMEBUFFERS);

    mnm::t_ctx->framebuffer_recorder.begin(u16(id));
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
    ASSERT(id > 0 && u16(id) < mnm::MAX_UNIFORMS);
    ASSERT(type > 0 && type <= UNIFORM_SAMPLER);
    ASSERT(count > 0);
    ASSERT(name);

    (void)mnm::g_ctx.uniform_cache.add(
        u16(id),
        u16(type),
        u16(count),
        name
    );
}

void uniform(int id, const void* value)
{
    using namespace mnm;

    ASSERT(id > 0 && u16(id) < MAX_UNIFORMS);
    ASSERT(value);

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder);
    }

    t_ctx->encoder->setUniform(g_ctx.uniform_cache[u16(id)].handle, value, UINT16_MAX);
}

void create_shader(int id, const void* vs_data, int vs_size, const void* fs_data, int fs_size)
{
    ASSERT(id > 0 && u16(id) < mnm::MAX_PROGRAMS);
    ASSERT(vs_data);
    ASSERT(vs_size > 0);
    ASSERT(fs_data);
    ASSERT(fs_size > 0);

    (void)mnm::g_ctx.program_cache.add(
        u16(id),
        vs_data,
        u32(vs_size),
        fs_data,
        u32(fs_size)
    );
}

void shader(int id)
{
    ASSERT(id > 0 && u16(id) < mnm::MAX_PROGRAMS);

    mnm::t_ctx->draw_state.program = mnm::g_ctx.program_cache[u16(id)];
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TRANSFORMATIONS
// -----------------------------------------------------------------------------

void view(void)
{
    using namespace mnm;

    g_ctx.pass_cache[t_ctx->active_pass].set_view(t_ctx->matrix_stack.top);
}

void projection(void)
{
    using namespace mnm;

    g_ctx.pass_cache[t_ctx->active_pass].set_projection(t_ctx->matrix_stack.top);
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
    mnm::t_ctx->matrix_stack.top = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near_, float far_)
{
    mnm::t_ctx->matrix_stack.multiply_top(
        HMM_Orthographic(left, right, bottom, top, near_, far_)
    );
}

void perspective(float fovy, float aspect, float near_, float far_)
{
    mnm::t_ctx->matrix_stack.multiply_top(
        HMM_Perspective(fovy, aspect, near_, far_)
    );
}

void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y,
    float at_z, float up_x, float up_y, float up_z)
{
    mnm::t_ctx->matrix_stack.multiply_top(
        HMM_LookAt( HMM_Vec3(eye_x, eye_y, eye_z), HMM_Vec3(at_x, at_y, at_z),
            HMM_Vec3(up_x, up_y, up_z))
    );
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
        (void)fwrite(data, size_t(bytes), 1, f); // TODO : Check return value and report error.
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

    save_bytes(file_name, string, int(strlen(string)));
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
    return int(mnm::g_ctx.frame_number);
}


// -----------------------------------------------------------------------------
// INTERNAL API
// -----------------------------------------------------------------------------

extern "C" GLFWwindow* mnm_get_window(void)
{
    return mnm::g_ctx.window.handle;
}
