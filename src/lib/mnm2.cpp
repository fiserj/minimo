#include <mnm/mnm.h>

#include <assert.h>               // assert
#include <stddef.h>               // ptrdiff_t, size_t
#include <stdint.h>               // *int*_t, UINT*_MAX
#include <string.h>               // memcpy, strcat, strcpy

#include <algorithm>              // max, transform
#include <atomic>                 // atomic
#include <array>                  // array
#include <chrono>                 // duration
#include <functional>             // hash
#include <mutex>                  // lock_guard, mutex
#include <thread>                 // this_thread
#include <type_traits>            // alignment_of, conditional, is_trivial, is_standard_layout
#include <vector>                 // vector

#include <bgfx/bgfx.h>            // bgfx::*
#include <bgfx/embedded_shader.h> // BGFX_EMBEDDED_SHADER*

#include <bx/bx.h>                // BX_ALIGN_DECL_16, BX_COUNTOF
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

#include <meshoptimizer.h>        // meshopt_*

#include <TaskScheduler.h>        // ITaskSet, TaskScheduler, TaskSetPartition

#include <mnm_shaders.h>          // *_fs, *_vs

namespace mnm
{

// -----------------------------------------------------------------------------
// FLAG MASKS AND SHIFTS
// -----------------------------------------------------------------------------

constexpr uint16_t MESH_TYPE_MASK        = MESH_TRANSIENT | MESH_DYNAMIC;

constexpr uint16_t MESH_TYPE_SHIFT       = 0;

constexpr uint16_t PRIMITIVE_TYPE_MASK   = PRIMITIVE_QUADS | PRIMITIVE_TRIANGLE_STRIP | PRIMITIVE_LINES | PRIMITIVE_LINE_STRIP | PRIMITIVE_POINTS;

constexpr uint16_t PRIMITIVE_TYPE_SHIFT  = 2;

constexpr uint16_t VERTEX_ATTRIB_MASK    = VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD;

constexpr uint16_t VERTEX_ATTRIB_SHIFT   = 4;


// -----------------------------------------------------------------------------
// RESOURCE LIMITS
// -----------------------------------------------------------------------------

constexpr uint32_t MAX_FRAMEBUFFERS      = 128;

constexpr uint32_t MAX_MESHES            = 4096;

constexpr uint32_t MAX_PASSES            = 64;

constexpr uint32_t MAX_TASKS             = 64;

constexpr uint32_t MAX_TEXTURES          = 1024;


// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

constexpr uint16_t MIN_WINDOW_SIZE       = 240;

constexpr uint16_t DEFAULT_WINDOW_WIDTH  = 800;

constexpr uint16_t DEFAULT_WINDOW_HEIGHT = 600;

enum
{
                   MESH_STATIC           = 0,

                   PRIMITIVE_TRIANGLES   = 0,

                   VERTEX_POSITION       = 0,
};


// -----------------------------------------------------------------------------
// UTILITY MACROS
// -----------------------------------------------------------------------------

// TODO : Better assert.
#define ASSERT(cond) assert(cond)


// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T, uint32_t Size>
using Array = std::array<T, Size>;

template <typename T>
using Atomic = std::atomic<T>;

using Mutex = std::mutex;

using MutexScope = std::lock_guard<std::mutex>;

template <typename T>
using Vector = std::vector<T>;

using Mat4 = hmm_mat4;

using Vec2 = hmm_vec2;

using Vec3 = hmm_vec3;

using Vec4 = hmm_vec4;


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
// DRAW SUBMISSION
// -----------------------------------------------------------------------------

struct DrawItem
{
    uint16_t                 transform     = UINT16_MAX;
    uint16_t                 mesh          = UINT16_MAX;
    bgfx::ViewId             pass          = UINT16_MAX;
    bgfx::FrameBufferHandle  framebuffer   = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      program       = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle      texture       = BGFX_INVALID_HANDLE; // TODO : More texture slots.
    bgfx::UniformHandle      sampler       = BGFX_INVALID_HANDLE;
};

class DrawList
{
public:
    inline void clear()
    {
        m_items   .clear();
        m_matrices.clear();
        m_state = {};
    }

    void submit_mesh(uint16_t mesh, const Mat4& transform)
    {
        m_state.transform = static_cast<uint16_t>(m_matrices.size());
        m_state.mesh      = mesh;

        m_matrices.push_back(transform);
        m_items   .push_back(m_state  );
        m_state = {};
    }

    inline DrawItem& state() { return m_state; }

    inline const DrawItem& state() const { return m_state; }

    inline const Vector<DrawItem>& items() const { return m_items; }

    inline const Vector<Mat4>& matrices() const { return m_matrices; }

private:
    DrawItem         m_state;
    Vector<DrawItem> m_items;
    Vector<Mat4>     m_matrices;
};


// -----------------------------------------------------------------------------
// PROGRAM CACHE
// -----------------------------------------------------------------------------

class ProgramCache
{
public:
    void clear()
    {
        for (bgfx::ProgramHandle handle : m_handles)
        {
            bgfx::destroy(handle);
        }

        m_handles       .clear();
        m_attribs_to_ids.clear();
    }

    uint8_t add(bgfx::ShaderHandle vertex, bgfx::ShaderHandle fragment, uint16_t attribs = UINT16_MAX)
    {
        if (m_handles.size() >= UINT8_MAX)
        {
            ASSERT(false && "Program cache full.");
            return UINT8_MAX;
        }

        if (!bgfx::isValid(vertex) || !bgfx::isValid(fragment))
        {
            ASSERT(false && "Invalid vertex and/or fragment shader.");
            return UINT8_MAX;
        }

        // TODO : Don't necessarily destroy shaders.
        bgfx::ProgramHandle handle = bgfx::createProgram(vertex, fragment, true);
        if (!bgfx::isValid( handle))
        {
            ASSERT(false && "Invalid program handle.");
            return UINT8_MAX;
        }

        const uint8_t idx = static_cast<uint8_t>(m_handles.size());

        if (attribs != UINT16_MAX)
        {
            attribs = (attribs & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT;
            ASSERT(attribs <  UINT8_MAX);

            if (attribs >= m_attribs_to_ids.size())
            {
                m_attribs_to_ids.resize(attribs + 1, UINT8_MAX);
            }

            if (m_attribs_to_ids[attribs] != UINT8_MAX)
            {
                ASSERT(false && "Default shader for given attributes already set.");
                bgfx::destroy(handle);
                return UINT8_MAX;
            }

            m_attribs_to_ids[attribs] = idx;
        }

        m_handles.push_back(handle);

        return idx;
    }

    inline uint8_t add
    (
        const bgfx::EmbeddedShader* shaders,
        bgfx::RendererType::Enum    renderer,
        const char*                 vertex_name,
        const char*                 fragment_name,
        uint16_t                    attribs = UINT16_MAX
    )
    {
        return add(
            bgfx::createEmbeddedShader(shaders, renderer, vertex_name  ),
            bgfx::createEmbeddedShader(shaders, renderer, fragment_name),
            attribs
        );
    }

    inline bgfx::ProgramHandle program_handle_from_id(uint8_t id) const
    {
        ASSERT(id < m_handles.size());
        ASSERT(bgfx::isValid(m_handles[id]));

        return m_handles[id];
    }

    inline bgfx::ProgramHandle program_handle_from_flags(uint16_t flags) const
    {
        const uint16_t attribs = (flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT;

        ASSERT(attribs < m_attribs_to_ids.size());
        ASSERT(m_attribs_to_ids[attribs] != UINT8_MAX);

        return program_handle_from_id(m_attribs_to_ids[attribs]);
    }

private:
    Vector<bgfx::ProgramHandle> m_handles;
    Vector<uint8_t>             m_attribs_to_ids;
};


// -----------------------------------------------------------------------------
// UNIFORMS
// -----------------------------------------------------------------------------

struct DefaultUniforms
{
    bgfx::UniformHandle color_texture = BGFX_INVALID_HANDLE;

    void init()
    {
        color_texture = bgfx::createUniform("s_tex_color", bgfx::UniformType::Sampler);
    }

    void clear()
    {
        destroy_if_valid(color_texture);
    }
};


// -----------------------------------------------------------------------------
// PASSES
// -----------------------------------------------------------------------------

class Pass
{
public:
    inline void update_view(bgfx::ViewId id, const Vector<Mat4>& matrices)
    {
        if (m_dirty_flags & DIRTY_CLEAR)
        {
            bgfx::setViewClear(id, m_clear_flags, m_clear_rgba, m_clear_depth, m_clear_stencil);
        }

        if (m_dirty_flags & DIRTY_TOUCH)
        {
            ASSERT(m_view_matrix_idx < matrices.size());
            ASSERT(m_proj_matrix_idx < matrices.size());

            bgfx::setViewTransform(id, &matrices[m_view_matrix_idx], &matrices[m_proj_matrix_idx]);
            bgfx::touch(id);

            m_view_matrix_idx = UINT16_MAX;
            m_proj_matrix_idx = UINT16_MAX;
        }

        if (m_dirty_flags & DIRTY_RECT)
        {
            ASSERT(m_viewport_width  != UINT16_MAX);
            ASSERT(m_viewport_height != UINT16_MAX);

            bgfx::setViewRect(id, m_viewport_x, m_viewport_y, m_viewport_width, m_viewport_height);
        }

        if (m_dirty_flags & DIRTY_FRAMEBUFFER)
        {
            // Having `BGFX_INVALID_HANDLE` here is OK.
            bgfx::setViewFrameBuffer(id, m_framebuffer);

            m_framebuffer = BGFX_INVALID_HANDLE;
        }

        m_dirty_flags = DIRTY_NONE;
    }

    inline void set_transform_indices(uint16_t view, uint16_t proj)
    {
        m_view_matrix_idx = view;
        m_proj_matrix_idx = proj;
        m_dirty_flags    |= DIRTY_TOUCH;
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
        if (m_clear_depth != depth)
        {
            m_clear_flags |= BGFX_CLEAR_DEPTH;
            m_clear_depth  = depth;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    void set_clear_color(uint32_t rgba)
    {
        if (m_clear_rgba != rgba)
        {
            m_clear_flags |= BGFX_CLEAR_COLOR;
            m_clear_rgba   = rgba;
            m_dirty_flags |= DIRTY_CLEAR;
        }
    }

    inline void set_viewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
    {
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
        DIRTY_NONE        = 0x0,
        DIRTY_CLEAR       = 0x1,
        DIRTY_TOUCH       = 0x2,
        DIRTY_RECT        = 0x4,
        DIRTY_FRAMEBUFFER = 0x8,
    };

private:
    uint16_t                m_view_matrix_idx = UINT16_MAX;
    uint16_t                m_proj_matrix_idx = UINT16_MAX;

    uint16_t                m_viewport_x      = 0;
    uint16_t                m_viewport_y      = 0;
    uint16_t                m_viewport_width  = UINT16_MAX;
    uint16_t                m_viewport_height = UINT16_MAX;

    bgfx::FrameBufferHandle m_framebuffer     = BGFX_INVALID_HANDLE;

    float                   m_clear_depth     = 1.0f;
    uint32_t                m_clear_rgba      = 0x000000ff;
    uint16_t                m_clear_flags     = BGFX_CLEAR_NONE;
    uint8_t                 m_clear_stencil   = 0;

    uint8_t                 m_dirty_flags     = DIRTY_CLEAR;
};


// -----------------------------------------------------------------------------
// VERTEX LAYOUT CACHE
// -----------------------------------------------------------------------------

class VertexLayoutCache
{
public:
    void add(uint16_t attribs)
    {
        if (attribs < m_layouts.size() && m_layouts[attribs].getStride() > 0)
        {
            return;
        }

        bgfx::VertexLayout layout;
        layout.begin();

        if (attribs == VERTEX_POSITION)
        {
            layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
        }

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
        ASSERT(layout.getStride() % 4 == 0);

        if (attribs >= m_layouts.size())
        {
            m_layouts.resize(attribs + 1);
        }

        m_layouts[attribs] = layout;

        return;
    }

    void add_builtins()
    {
        add(VERTEX_POSITION);

        add(VERTEX_COLOR   );
        add(VERTEX_NORMAL  );
        add(VERTEX_TEXCOORD);

        add(VERTEX_COLOR  | VERTEX_NORMAL  );
        add(VERTEX_COLOR  | VERTEX_TEXCOORD);
        add(VERTEX_NORMAL | VERTEX_TEXCOORD);

        add(VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD);
    }

    inline const bgfx::VertexLayout& operator[](uint16_t attribs) const
    {
        ASSERT(attribs < m_layouts.size());
        return m_layouts[attribs];
    }

    inline void clear()
    {
        m_layouts.clear();
    }

protected:
    Vector<bgfx::VertexLayout> m_layouts;
};


// -----------------------------------------------------------------------------
// VERTEX ATTRIB STATE
// -----------------------------------------------------------------------------

BX_ALIGN_DECL_16(struct) VertexAttribState
{
    uint8_t data[32];

    using ColorType    = uint32_t; // RGBA_u8.

    using NormalType   = uint32_t; // Packed as RGB_u8.

    using TexcoordType = uint32_t; // Packed as RG_s16.

    template <typename ReturnT, size_t BytesOffset>
    const ReturnT* at() const
    {
        static_assert(is_pod<ReturnT>(),
            "ReturnT must be POD type.");

        static_assert(BytesOffset % sizeof(ReturnT) == 0,
            "BytesOffset must be multiple of sizeof(ReturnT).");

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
        size += sizeof(VertexAttribState::ColorType);
    }

    if constexpr (!!(Flags & VERTEX_NORMAL))
    {
        size += sizeof(VertexAttribState::NormalType);
    }

    if constexpr (!!(Flags & VERTEX_TEXCOORD))
    {
        size += sizeof(VertexAttribState::TexcoordType);
    }

    return size;
}

template <uint16_t Flags, uint16_t Attrib>
static constexpr size_t vertex_attrib_offset()
{
    static_assert(
        Attrib == VERTEX_COLOR  ||
        Attrib == VERTEX_NORMAL ||
        Attrib == VERTEX_TEXCOORD,
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
        offset += sizeof(VertexAttribState::ColorType);
    }

    if constexpr (Attrib != VERTEX_NORMAL && (Flags & VERTEX_NORMAL))
    {
        offset += sizeof(VertexAttribState::NormalType);
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
        add<VERTEX_POSITION>();

        add<VERTEX_COLOR   >();
        add<VERTEX_NORMAL  >();
        add<VERTEX_TEXCOORD>();

        add<VERTEX_COLOR  | VERTEX_NORMAL  >();
        add<VERTEX_COLOR  | VERTEX_TEXCOORD>();
        add<VERTEX_NORMAL | VERTEX_TEXCOORD>();

        add<VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD>();
    }

    inline const VertexAttribStateFuncSet& operator[](uint16_t flags) const
    {
        return m_func_sets[flags & VERTEX_ATTRIB_MASK];
    }

private:
    template <uint16_t Flags>
    static void color(VertexAttribState& state, uint32_t rgba)
    {
        if constexpr (!!(Flags & VERTEX_COLOR))
        {
            *state.at<VertexAttribState::ColorType, vertex_attrib_offset<Flags, VERTEX_COLOR>()>() = bx::endianSwap(rgba);
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

            bx::packRgb8(state.at<VertexAttribState::NormalType, vertex_attrib_offset<Flags, VERTEX_NORMAL>()>(), normalized);
        }
    }

    template <uint16_t Flags>
    static void texcoord(VertexAttribState& state, float u, float v)
    {
        if constexpr (!!(Flags & VERTEX_TEXCOORD))
        {
            const float elems[] = { u, v };

            bx::packRg16S(state.at<VertexAttribState::TexcoordType, vertex_attrib_offset<Flags, VERTEX_TEXCOORD>()>(), elems);
        }
    }

    template <uint16_t Flags>
    void add()
    {
        VertexAttribStateFuncSet func_set;

        func_set.color    = color   <Flags>;
        func_set.normal   = normal  <Flags>;
        func_set.texcoord = texcoord<Flags>;

        if (m_func_sets.size() <= Flags)
        {
            m_func_sets.resize(Flags + 1);
        }

        m_func_sets[Flags] = func_set;
    }

private:
    Vector<VertexAttribStateFuncSet> m_func_sets;
};


// -----------------------------------------------------------------------------
// GEOMETRY RECORDING
// -----------------------------------------------------------------------------

class GeometryRecorder
{
public:
    void reset(uint16_t flags)
    {
        m_position_buffer.clear();
        m_attrib_buffer  .clear();

        m_attrib_funcs     = &ms_attrib_state_func_table[flags];
        m_vertex_func      =  ms_vertex_push_func_table [flags];
        m_vertex_count     = 0;
        m_invocation_count = 0;
    }

    inline void vertex(const Vec3& position)
    {
        (* m_vertex_func)(*this, position);
    }

    inline void color(uint32_t rgba)
    {
        m_attrib_funcs->color(m_attrib_state, rgba);
    }

    inline void normal(float nx, float ny, float nz)
    {
        m_attrib_funcs->normal(m_attrib_state, nx, ny, nz);
    }

    inline void texcoord(float u, float v)
    {
        m_attrib_funcs->texcoord(m_attrib_state, u, v);
    }

    inline uint32_t vertex_count() const { return m_vertex_count; }

    inline const Vector<uint8_t>& attrib_buffer() const { return m_attrib_buffer; }

    inline const Vector<uint8_t>& position_buffer() const { return m_position_buffer; }

private:
    using VertexPushFunc = void (*)(GeometryRecorder&, const Vec3&);

    class VertexPushFuncTable
    {
    public:
        VertexPushFuncTable()
        {
            add<VERTEX_POSITION>();

            add<VERTEX_COLOR   >();
            add<VERTEX_NORMAL  >();
            add<VERTEX_TEXCOORD>();

            add<VERTEX_COLOR  | VERTEX_NORMAL  >();
            add<VERTEX_COLOR  | VERTEX_TEXCOORD>();
            add<VERTEX_NORMAL | VERTEX_TEXCOORD>();

            add<VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD>();

            add<PRIMITIVE_QUADS | VERTEX_POSITION>();

            add<PRIMITIVE_QUADS | VERTEX_COLOR   >();
            add<PRIMITIVE_QUADS | VERTEX_NORMAL  >();
            add<PRIMITIVE_QUADS | VERTEX_TEXCOORD>();

            add<PRIMITIVE_QUADS | VERTEX_COLOR  | VERTEX_NORMAL  >();
            add<PRIMITIVE_QUADS | VERTEX_COLOR  | VERTEX_TEXCOORD>();
            add<PRIMITIVE_QUADS | VERTEX_NORMAL | VERTEX_TEXCOORD>();

            add<PRIMITIVE_QUADS | VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD>();
        }

        inline const VertexPushFunc& operator[](uint16_t flags) const
        {
            // Clear mesh type and primitive type flags, except the quad primitive bit.
            return m_funcs[flags & ~MESH_TYPE_MASK & (~PRIMITIVE_TYPE_MASK | PRIMITIVE_QUADS)];
        }

    private:
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
        static void vertex(GeometryRecorder& recorder, const Vec3& position)
        {
            if constexpr (!!(Flags & (PRIMITIVE_QUADS)))
            {
                if ((recorder.m_invocation_count & 3) == 3)
                {
                    emulate_quad<sizeof(position)>(recorder.m_position_buffer);

                    if constexpr (!!(Flags & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)))
                    {
                        emulate_quad<vertex_attribs_size<Flags>()>(recorder.m_attrib_buffer);
                    }

                    recorder.m_vertex_count += 2;
                }

                recorder.m_invocation_count++;
            }

            recorder.m_vertex_count++;

            push_back(recorder.m_position_buffer, position);

            if constexpr (!!(Flags & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)))
            {
                push_back<vertex_attribs_size<Flags>()>(recorder.m_attrib_buffer, recorder.m_attrib_state.data);
            }
        }

        template <uint16_t Flags>
        void add()
        {
            if (m_funcs.size() <= Flags)
            {
                m_funcs.resize(Flags + 1, nullptr);
            }

            m_funcs[Flags] = vertex<Flags>;
        }

    private:
        Vector<VertexPushFunc> m_funcs;
    };

protected:
    Vector<uint8_t>                         m_attrib_buffer;
    Vector<uint8_t>                         m_position_buffer;
    VertexAttribState                       m_attrib_state;
    const VertexAttribStateFuncSet*         m_attrib_funcs     = nullptr;
    VertexPushFunc                          m_vertex_func      = nullptr;
    uint32_t                                m_vertex_count     = 0;
    uint32_t                                m_invocation_count = 0;

    static const VertexAttribStateFuncTable ms_attrib_state_func_table;
    static const VertexPushFuncTable        ms_vertex_push_func_table;
};

const VertexAttribStateFuncTable GeometryRecorder::ms_attrib_state_func_table;

const GeometryRecorder::VertexPushFuncTable GeometryRecorder::ms_vertex_push_func_table;


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
    uint16_t          flags         = 0;
    VertexBufferUnion positions;
    VertexBufferUnion attribs;
    IndexBufferUnion  indices;

    static inline uint16_t type(uint16_t flags)
    {
        return (flags & MESH_TYPE_MASK);
    }

    inline uint16_t type() const
    {
        return type(flags);
    }
};


// -----------------------------------------------------------------------------
// MESH CACHE
// -----------------------------------------------------------------------------

struct MeshCache
{
public:
    bool add_mesh(uint16_t id, uint16_t flags, const GeometryRecorder& recorder, const VertexLayoutCache& vertex_layout_cache)
    {
        ASSERT(id < m_meshes.size());

        MutexScope lock(m_mutex);

        Mesh& mesh = m_meshes[id];

        const uint16_t old_type = mesh. type();
        const uint16_t new_type = Mesh::type(flags);

        switch (old_type)
        {
        case MESH_TRANSIENT:
            ASSERT(mesh.positions.transient_index == bgfx::kInvalidHandle &&
                "Do you really want to overwrite transient mesh with " \
                "this ID, that was also defined in the current frame?");
            break;

        case MESH_STATIC:
            destroy_static_mesh(mesh);
            break;

        case MESH_DYNAMIC:
            destroy_dynamic_mesh(mesh);
            break;

        default:
            break;
        }

        mesh.element_count = recorder.vertex_count();
        mesh.flags         = flags;

        switch (new_type)
        {
        case MESH_TRANSIENT:
            // add_transient_mesh(mesh, recorder, vertex_layout_cache);
            m_transient_mesh_idxs.push_back(id);
            break;

        case MESH_STATIC:
            // add_persistent_mesh<MESH_STATIC>(mesh, recorder, vertex_layout_cache);
            break;

        case MESH_DYNAMIC:
            // add_persistent_mesh<MESH_DYNAMIC>(mesh, recorder, vertex_layout_cache);
            break;

        default:
            break;
        }

        return true;
    }

    void clear_transient_meshes()
    {
        MutexScope lock(m_mutex);

        for (uint16_t idx : m_transient_mesh_idxs)
        {
            ASSERT(m_meshes[idx].type() == MESH_TRANSIENT);
            m_meshes[idx] = {};
        }

        m_transient_mesh_idxs     .clear();
        m_transient_vertex_buffers.clear();
    }

    void clear_persistent_meshes()
    {
        MutexScope lock(m_mutex);

        for (Mesh& mesh : m_meshes)
        {
            switch (mesh.type())
            {
            case MESH_STATIC:
                destroy_static_mesh(mesh);
                break;

            case MESH_DYNAMIC:
                destroy_dynamic_mesh(mesh);
                break;

            default:
                break;
            }
        }
    }

    inline Mesh& operator[](uint16_t id) { return m_meshes[id]; }

    inline const Mesh& operator[](uint16_t id) const { return m_meshes[id]; }

    inline const Vector<bgfx::TransientVertexBuffer>& transient_vertex_buffers() const
    {
        return m_transient_vertex_buffers;
    }

private:
    static inline void destroy_static_mesh(Mesh& mesh)
    {
        ASSERT(bgfx::isValid(mesh.positions.static_buffer));
        ASSERT(bgfx::isValid(mesh.indices  .static_buffer));

        bgfx::destroy(mesh.positions.static_buffer);
        bgfx::destroy(mesh.attribs  .static_buffer);

        destroy_if_valid(mesh.indices.static_buffer);

        mesh = {};
    }

    static inline void destroy_dynamic_mesh(Mesh& mesh)
    {
        ASSERT(bgfx::isValid(mesh.positions.dynamic_buffer));
        ASSERT(bgfx::isValid(mesh.indices  .dynamic_buffer));

        bgfx::destroy(mesh.positions.dynamic_buffer);
        bgfx::destroy(mesh.attribs  .dynamic_buffer);

        destroy_if_valid(mesh.indices.dynamic_buffer);

        mesh = {};
    }
    // template <uint16_t MeshType>
    // static inline void destroy_mesh(Mesh& mesh)
    // {
    //     static_assert(MeshType == MESH_STATIC || MeshType == MESH_DYNAMIC, "Unsupported mesh type for destruction.");

    //     if
    //     destroy_if_valid<VertexBufferT>(mesh.position_buffer);
    //     destroy_if_valid<VertexBufferT>(mesh.attrib_buffer  );
    //     destroy_if_valid<IndexBufferT >(mesh.index_buffer   );

    //     mesh = {};
    // }

    // inline void add_transient_mesh(Mesh& mesh, const GeometryRecorder& recorder, const VertexLayoutCache& vertex_layout_cache)
    // {
    //     if (!(
    //         update_transient_buffer(mesh.element_count, recorder.position_buffer(), vertex_layout_cache[VERTEX_POSITION], mesh.position_buffer) &&
    //         update_transient_buffer(mesh.element_count, recorder.attrib_buffer  (), vertex_layout_cache[mesh.attribs() ], mesh.attrib_buffer  )
    //     ))
    //     {
    //         mesh = {}; // TODO : We need to handle this in a reasonable way.
    //     }
    // }

    // bool update_transient_buffer
    // (
    //     uint32_t                  vertex_count,
    //     const Vector<uint8_t>&    data,
    //     const bgfx::VertexLayout& vertex_layout,
    //     uint16_t&                 dst_buffer_index
    // )
    // {
    //     ASSERT(data.size() % vertex_layout.getStride() == 0);
    //     ASSERT(data.size() / vertex_layout.getStride() == vertex_count);

    //     bool success = true;

    //     if (vertex_count > 0)
    //     {
    //         if (bgfx::getAvailTransientVertexBuffer(vertex_count, vertex_layout) >= vertex_count)
    //         {
    //             ASSERT(m_transient_vertex_buffers.size() < UINT16_MAX);
    //             dst_buffer_index = static_cast<uint16_t>(m_transient_vertex_buffers.size());

    //             m_transient_vertex_buffers.resize(m_transient_vertex_buffers.size() + 1);
    //             bgfx::TransientVertexBuffer& buffer = m_transient_vertex_buffers.back();

    //             bgfx::allocTransientVertexBuffer(&buffer, vertex_count, vertex_layout);
    //             (void)memcpy(buffer.data, data.data(), data.size());
    //         }
    //         else
    //         {
    //             ASSERT(false && "Unable to allocate requested number of transient vertices.");
    //             success = false; // TODO : We need to handle this in a reasonable way.
    //         }
    //     }
    //     else
    //     {
    //         dst_buffer_index = UINT16_MAX;
    //     }

    //     return success;
    // }

    // template <MeshType MeshType>
    // void add_persistent_mesh(Mesh& mesh, const GeometryRecorder& recorder, const VertexLayoutCache& vertex_layout_cache)
    // {
    //     static_assert(MeshType == MESH_STATIC || MeshType == MESH_DYNAMIC, "Unsupported mesh type for destruction.");

    //     using VertexBufferT = typename std::conditional<MeshType == MESH_STATIC, bgfx::VertexBufferHandle, bgfx::DynamicVertexBufferHandle>::type;
    //     using IndexBufferT  = typename std::conditional<MeshType == MESH_STATIC, bgfx::IndexBufferHandle , bgfx::DynamicIndexBufferHandle >::type;

    //     meshopt_Stream            streams[2];
    //     const bgfx::VertexLayout* layouts[2];

    //     layouts[0] = &vertex_layout_cache[VERTEX_POSITION]; // TODO : Eventually add support for 2D position.
    //     streams[0] = { recorder.position_buffer().data(), layouts[0]->getStride(), layouts[0]->getStride() };

    //     const bool has_attribs = (mesh.attribs() & VERTEX_ATTRIB_MASK);

    //     if (has_attribs)
    //     {
    //         layouts[1] = &vertex_layout_cache[mesh.attribs()];
    //         streams[1] = { recorder.attrib_buffer().data(), layouts[1]->getStride(), layouts[1]->getStride() };
    //     }

    //     Vector<unsigned int> remap_table(mesh.element_count);
    //     uint32_t             indexed_vertex_count = 0;

    //     if (has_attribs)
    //     {
    //         indexed_vertex_count = static_cast<uint32_t>(meshopt_generateVertexRemapMulti(
    //             remap_table.data(), nullptr, mesh.element_count, mesh.element_count, streams, BX_COUNTOF(streams)
    //         ));

    //         update_persistent_vertex_buffer<VertexBufferT>(
    //             streams[1], *layouts[1], mesh.element_count, indexed_vertex_count, remap_table, mesh.attrib_buffer
    //         );
    //     }
    //     else
    //     {
    //         indexed_vertex_count = static_cast<uint32_t>(meshopt_generateVertexRemap(
    //             remap_table.data(), nullptr, mesh.element_count, streams[0].data, mesh.element_count, streams[0].size
    //         ));
    //     }

    //     void* vertex_positions = nullptr;
    //     update_persistent_vertex_buffer<VertexBufferT>(
    //         streams[0], *layouts[0], mesh.element_count, indexed_vertex_count, remap_table, mesh.position_buffer, &vertex_positions
    //     );

    //     update_persistent_index_buffer<IndexBufferT>(
    //         mesh.element_count,
    //         indexed_vertex_count,
    //         remap_table,
    //         (mesh.flags & PRIMITIVE_TYPE_MASK) <= PRIMITIVE_QUADS,
    //         static_cast<float*>(vertex_positions),
    //         mesh.index_buffer
    //     );
    // }

    // template <typename BufferT>
    // inline static void update_persistent_vertex_buffer
    // (
    //     const meshopt_Stream&       stream,
    //     const bgfx::VertexLayout&   layout,
    //     uint32_t                    vertex_count,
    //     uint32_t                    indexed_vertex_count,
    //     const Vector<unsigned int>& remap_table,
    //     uint16_t&                   dst_buffer_handle,
    //     void**                      dst_remapped_memory = nullptr
    // )
    // {
    //     static_assert(
    //         std::is_same<BufferT, bgfx::       VertexBufferHandle>::value ||
    //         std::is_same<BufferT, bgfx::DynamicVertexBufferHandle>::value,
    //         "Unsupported vertex buffer type for update."
    //     );

    //     const bgfx::Memory* memory = bgfx::alloc(static_cast<uint32_t>(indexed_vertex_count * stream.size));
    //     ASSERT(memory && memory->data);

    //     meshopt_remapVertexBuffer(memory->data, stream.data, vertex_count, stream.size, remap_table.data());

    //     if (dst_remapped_memory)
    //     {
    //         *dst_remapped_memory = memory->data;
    //     }

    //     if constexpr (std::is_same<BufferT, bgfx::VertexBufferHandle>::value)
    //     {
    //         dst_buffer_handle = bgfx::createVertexBuffer(memory, layout).idx;
    //     }

    //     if constexpr (std::is_same<BufferT, bgfx::DynamicVertexBufferHandle>::value)
    //     {
    //         dst_buffer_handle = bgfx::createDynamicVertexBuffer(memory, layout).idx;
    //     }

    //     ASSERT(dst_buffer_handle != bgfx::kInvalidHandle);
    // }

    // template <typename T>
    // inline static void remap_index_buffer
    // (
    //     uint32_t                    vertex_count,
    //     uint32_t                    indexed_vertex_count,
    //     const Vector<unsigned int>& remap_table,
    //     bool                        optimize,
    //     const float*                vertex_positions,
    //     T*                          dst_indices
    // )
    // {
    //     meshopt_remapIndexBuffer<T>(dst_indices, nullptr, vertex_count, remap_table.data());

    //     if (optimize && vertex_positions)
    //     {
    //         meshopt_optimizeVertexCache<T>(dst_indices, dst_indices, vertex_count, indexed_vertex_count);

    //         meshopt_optimizeOverdraw(dst_indices, dst_indices, vertex_count, vertex_positions, indexed_vertex_count, 3 * sizeof(float), 1.05f);
    //     }
    // }

    // template <typename BufferT>
    // inline static void update_persistent_index_buffer
    // (
    //     uint32_t                    vertex_count,
    //     uint32_t                    indexed_vertex_count,
    //     const Vector<unsigned int>& remap_table,
    //     bool                        optimize,
    //     const float*                vertex_positions,
    //     uint16_t&                   dst_buffer_handle
    // )
    // {
    //     static_assert(
    //         std::is_same<BufferT, bgfx::       IndexBufferHandle>::value ||
    //         std::is_same<BufferT, bgfx::DynamicIndexBufferHandle>::value,
    //         "Unsupported index buffer type for update."
    //     );

    //     uint16_t buffer_flags = BGFX_BUFFER_NONE;
    //     uint32_t type_size    = sizeof(uint16_t);

    //     if (indexed_vertex_count > UINT16_MAX)
    //     {
    //         buffer_flags = BGFX_BUFFER_INDEX32;
    //         type_size    = sizeof(uint32_t);
    //     }

    //     const bgfx::Memory* memory = bgfx::alloc(vertex_count * type_size);
    //     ASSERT(memory && memory->data);

    //     type_size == sizeof(uint16_t)
    //         ? remap_index_buffer(vertex_count, indexed_vertex_count, remap_table, optimize, vertex_positions, reinterpret_cast<uint16_t*>(memory->data))
    //         : remap_index_buffer(vertex_count, indexed_vertex_count, remap_table, optimize, vertex_positions, reinterpret_cast<uint32_t*>(memory->data));

    //     if constexpr (std::is_same<BufferT, bgfx::IndexBufferHandle>::value)
    //     {
    //         dst_buffer_handle = bgfx::createIndexBuffer(memory, buffer_flags).idx;
    //     }

    //     if constexpr (std::is_same<BufferT, bgfx::DynamicIndexBufferHandle>::value)
    //     {
    //         dst_buffer_handle = bgfx::createDynamicIndexBuffer(memory, buffer_flags).idx;
    //     }

    //     ASSERT(dst_buffer_handle != bgfx::kInvalidHandle);
    // }

private:
    Mutex                               m_mutex;
    Array<Mesh, MAX_MESHES>             m_meshes;
    Vector<uint16_t>                    m_transient_mesh_idxs;
    Vector<bgfx::TransientVertexBuffer> m_transient_vertex_buffers;
};



} // namespace mnm
