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
// CONSTANTS
// -----------------------------------------------------------------------------

enum MeshType
{
    MESH_INVALID   = 0x000,
    MESH_TRANSIENT = 0x040,
    MESH_STATIC    = 0x080,
    MESH_DYNAMIC   = 0x100,
};

enum
{
    VERTEX_POSITION = 0,
};

enum
{
    PRIMITIVE_TRIANGLES = 0,
};

// constexpr uint32_t VERTEX_ATTRIB_SHIFT   = 0;

constexpr uint32_t VERTEX_ATTRIB_MASK    = VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD;

constexpr uint32_t PRIMITIVE_TYPE_SHIFT  = 3;

constexpr uint32_t PRIMITIVE_TYPE_MASK   = PRIMITIVE_QUADS | PRIMITIVE_TRIANGLE_STRIP | PRIMITIVE_LINES | PRIMITIVE_LINE_STRIP;

// constexpr uint32_t MESH_TYPE_SHIFT       = 5;

constexpr uint32_t MESH_TYPE_MASK        = MESH_TRANSIENT | MESH_STATIC;

constexpr uint32_t MAX_MESHES            = 4096;

constexpr uint32_t MAX_TEXTURES          = 4096;

constexpr uint32_t MAX_PROGRAM_UNIFORMS  = 8;

constexpr uint32_t MAX_PASSES            = 64;

constexpr uint32_t MAX_FRAMEBUFFERS      = 128;

constexpr uint32_t MAX_TASKS             = 64;

constexpr uint16_t MIN_WINDOW_SIZE       = 240;

constexpr uint16_t DEFAULT_WINDOW_WIDTH  = 800;

constexpr uint16_t DEFAULT_WINDOW_HEIGHT = 600;

constexpr bgfx::ViewId DEFAULT_PASS      = 0; // Should maybe set to be the last one?


// -----------------------------------------------------------------------------
// UTILITY MACROS
// -----------------------------------------------------------------------------

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

class PassStack : public Stack<bgfx::ViewId>
{
public:
    inline PassStack()
        : Stack<bgfx::ViewId>(DEFAULT_PASS)
    {
        push();
    }

    inline void top_and_push(bgfx::ViewId pass)
    {
        top() = pass;
        push();
    }

    inline void pop()
    {
        ASSERT(m_data.size() > 1);
        Stack<bgfx::ViewId>::pop();
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

template <typename T>
constexpr bool is_pod()
{
    // Since std::is_pod is being deprecated as of C++20.
    return std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}

template <typename HandleT>
inline void destroy_if_valid(uint16_t& handle_idx)
{
    if (bgfx::isValid(HandleT { handle_idx }))
    {
        bgfx::destroy(HandleT { handle_idx });
        handle_idx = bgfx::kInvalidHandle;
    }
}

template <typename HandleT>
inline void destroy_if_valid(HandleT& handle)
{
    destroy_if_valid<HandleT>(handle.idx);
}


// -----------------------------------------------------------------------------
// DRAW SUBMISSION
// -----------------------------------------------------------------------------

struct DrawItem
{
    // TODO : Mesh ID and view ID could be merged to single uint16_t.
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

    // NOTE : attribs here aren't shifted!
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
        const uint16_t attribs = flags & VERTEX_ATTRIB_MASK;

        ASSERT(attribs < m_attribs_to_ids.size());
        ASSERT(m_attribs_to_ids[attribs] != UINT8_MAX);

        return program_handle_from_id(m_attribs_to_ids[attribs]);
    }

private:
    Vector<bgfx::ProgramHandle> m_handles;
    Vector<uint8_t>             m_attribs_to_ids;
};

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
// PASS CACHE
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

    inline void set_framebuffer(const Framebuffer& framebuffer)
    {
        m_framebuffer  = framebuffer.handle;
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

    inline bgfx::FrameBufferHandle framebuffer() const
    {
        return m_framebuffer;
    }

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
    uint16_t m_view_matrix_idx = UINT16_MAX;
    uint16_t m_proj_matrix_idx = UINT16_MAX;

    uint16_t m_viewport_x      = 0;
    uint16_t m_viewport_y      = 0;
    uint16_t m_viewport_width  = UINT16_MAX;
    uint16_t m_viewport_height = UINT16_MAX;

    bgfx::FrameBufferHandle m_framebuffer = BGFX_INVALID_HANDLE;

    float    m_clear_depth     = 1.0f;
    uint32_t m_clear_rgba      = 0x000000ff;
    uint16_t m_clear_flags     = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH;
    uint8_t  m_clear_stencil   = 0;

    uint8_t  m_dirty_flags     = DIRTY_CLEAR;
};

class PassCache
{
public:
    void update()
    {
        MutexScope lock(m_mutex);

        for (bgfx::ViewId id = 0; id < m_passes.size(); id++)
        {
            m_passes[id].update_view(id, m_matrices);
        }

        m_matrices.clear();
    }

    void set_pass_transforms(bgfx::ViewId id, const Mat4& view, const Mat4& proj)
    {
        MutexScope lock(m_mutex);

        ASSERT(m_matrices.size() + 2 < UINT16_MAX);

        const uint16_t idx = static_cast<uint16_t>(m_matrices.size());

        m_passes[id].set_transform_indices(idx, idx + 1);

        m_matrices.push_back(view);
        m_matrices.push_back(proj);
    }

    // Changing pass properties directly is not thread safe, but it seems
    // super silly to actually attempt to do so from multiple threads.

    inline Pass& operator[](bgfx::ViewId i) { return m_passes[i]; }

    inline const Pass& operator[](bgfx::ViewId i) const { return m_passes[i]; }

private:
    Mutex                   m_mutex;
    Array<Pass, MAX_PASSES> m_passes;
    Vector<Mat4>            m_matrices;
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

    void clear()
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

static inline bool is_aligned(const void* ptr, size_t size)
{
    return reinterpret_cast<uintptr_t>(ptr) % size == 0;
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
// INSTANCE DATA RECORDING
// -----------------------------------------------------------------------------

class InstanceRecorder
{
public:
    void reset(uint16_t flags)
    {
        // ...  
    }

private:
};


// -----------------------------------------------------------------------------
// MESH
// -----------------------------------------------------------------------------

struct Mesh
{
    uint32_t element_count   = 0;
    uint16_t flags           = 0;
    uint16_t position_buffer = bgfx::kInvalidHandle;
    uint16_t attrib_buffer   = bgfx::kInvalidHandle;
    uint16_t index_buffer    = bgfx::kInvalidHandle;

    static inline MeshType type(uint16_t flags)
    {
        return static_cast<MeshType>(flags & MESH_TYPE_MASK);
    }

    static inline uint16_t attribs(uint16_t flags)
    {
        return flags & VERTEX_ATTRIB_MASK;
    }

    inline MeshType type() const { return type(flags); }

    inline uint16_t attribs() const { return attribs(flags); }
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

        const MeshType old_type = mesh. type();
        const MeshType new_type = Mesh::type(flags);

        if (new_type == MESH_INVALID)
        {
            ASSERT(false && "Invalid registered mesh type.");
            return false;
        }

        switch (old_type)
        {
        case MESH_TRANSIENT:
            ASSERT(false && "Do you really want to overwrite transient mesh " \
                "with this ID, that was also defined in the current frame?");
            break;

        case MESH_STATIC:
            destroy_mesh<MESH_STATIC>(mesh);
            break;

        case MESH_DYNAMIC:
            destroy_mesh<MESH_DYNAMIC>(mesh);
            break;

        default:
            break;
        }

        mesh.element_count = recorder.vertex_count();
        mesh.flags         = flags;

        switch (new_type)
        {
        case MESH_TRANSIENT:
            add_transient_mesh(mesh, recorder, vertex_layout_cache);
            m_transient_mesh_idxs.push_back(id);
            break;

        case MESH_STATIC:
            add_persistent_mesh<MESH_STATIC>(mesh, recorder, vertex_layout_cache);
            break;

        case MESH_DYNAMIC:
            add_persistent_mesh<MESH_DYNAMIC>(mesh, recorder, vertex_layout_cache);
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
                destroy_mesh<MESH_STATIC>(mesh);
                break;

            case MESH_DYNAMIC:
                destroy_mesh<MESH_DYNAMIC>(mesh);
                break;

            default:
                break;
            }
        }
    }

    inline Mesh& mesh(uint16_t id) { return m_meshes[id]; }

    inline const Mesh& mesh(uint16_t id) const { return m_meshes[id]; }

    inline const Vector<bgfx::TransientVertexBuffer>& transient_vertex_buffers() const { return m_transient_vertex_buffers; }

private:
    template <MeshType MeshType>
    static inline void destroy_mesh(Mesh& mesh)
    {
        static_assert(MeshType == MESH_STATIC || MeshType == MESH_DYNAMIC, "Unsupported mesh type for destruction.");

        using VertexBufferT = typename std::conditional<MeshType == MESH_STATIC, bgfx::VertexBufferHandle, bgfx::DynamicVertexBufferHandle>::type;
        using IndexBufferT  = typename std::conditional<MeshType == MESH_STATIC, bgfx::IndexBufferHandle , bgfx::DynamicIndexBufferHandle >::type;

        ASSERT(bgfx::isValid(VertexBufferT { mesh.position_buffer }));
        ASSERT(bgfx::isValid(IndexBufferT  { mesh.index_buffer    }));

        destroy_if_valid<VertexBufferT>(mesh.position_buffer);
        destroy_if_valid<VertexBufferT>(mesh.attrib_buffer  );
        destroy_if_valid<IndexBufferT >(mesh.index_buffer   );

        mesh = {};
    }

    inline void add_transient_mesh(Mesh& mesh, const GeometryRecorder& recorder, const VertexLayoutCache& vertex_layout_cache)
    {
        if (!(
            update_transient_buffer(mesh.element_count, recorder.position_buffer(), vertex_layout_cache[VERTEX_POSITION], mesh.position_buffer) &&
            update_transient_buffer(mesh.element_count, recorder.attrib_buffer  (), vertex_layout_cache[mesh.attribs() ], mesh.attrib_buffer  )
        ))
        {
            mesh = {}; // TODO : We need to handle this in a reasonable way.
        }
    }

    bool update_transient_buffer
    (
        uint32_t                  vertex_count,
        const Vector<uint8_t>&    data,
        const bgfx::VertexLayout& vertex_layout,
        uint16_t&                 dst_buffer_index
    )
    {
        ASSERT(data.size() % vertex_layout.getStride() == 0);
        ASSERT(data.size() / vertex_layout.getStride() == vertex_count);

        bool success = true;

        if (vertex_count > 0)
        {
            if (bgfx::getAvailTransientVertexBuffer(vertex_count, vertex_layout) >= vertex_count)
            {
                ASSERT(m_transient_vertex_buffers.size() < UINT16_MAX);
                dst_buffer_index = static_cast<uint16_t>(m_transient_vertex_buffers.size());

                m_transient_vertex_buffers.resize(m_transient_vertex_buffers.size() + 1);
                bgfx::TransientVertexBuffer& buffer = m_transient_vertex_buffers.back();

                bgfx::allocTransientVertexBuffer(&buffer, vertex_count, vertex_layout);
                (void)memcpy(buffer.data, data.data(), data.size());
            }
            else
            {
                ASSERT(false && "Unable to allocate requested number of transient vertices.");
                success = false; // TODO : We need to handle this in a reasonable way.
            }
        }
        else
        {
            dst_buffer_index = UINT16_MAX;
        }

        return success;
    }

    template <MeshType MeshType>
    void add_persistent_mesh(Mesh& mesh, const GeometryRecorder& recorder, const VertexLayoutCache& vertex_layout_cache)
    {
        static_assert(MeshType == MESH_STATIC || MeshType == MESH_DYNAMIC, "Unsupported mesh type for destruction.");

        using VertexBufferT = typename std::conditional<MeshType == MESH_STATIC, bgfx::VertexBufferHandle, bgfx::DynamicVertexBufferHandle>::type;
        using IndexBufferT  = typename std::conditional<MeshType == MESH_STATIC, bgfx::IndexBufferHandle , bgfx::DynamicIndexBufferHandle >::type;

        meshopt_Stream            streams[2];
        const bgfx::VertexLayout* layouts[2];

        layouts[0] = &vertex_layout_cache[VERTEX_POSITION]; // TODO : Eventually add support for 2D position.
        streams[0] = { recorder.position_buffer().data(), layouts[0]->getStride(), layouts[0]->getStride() };

        const bool has_attribs = (mesh.attribs() & VERTEX_ATTRIB_MASK);

        if (has_attribs)
        {
            layouts[1] = &vertex_layout_cache[mesh.attribs()];
            streams[1] = { recorder.attrib_buffer().data(), layouts[1]->getStride(), layouts[1]->getStride() };
        }

        Vector<unsigned int> remap_table(mesh.element_count);
        uint32_t             indexed_vertex_count = 0;

        if (has_attribs)
        {
            indexed_vertex_count = static_cast<uint32_t>(meshopt_generateVertexRemapMulti(
                remap_table.data(), nullptr, mesh.element_count, mesh.element_count, streams, BX_COUNTOF(streams)
            ));

            update_persistent_vertex_buffer<VertexBufferT>(
                streams[1], *layouts[1], mesh.element_count, indexed_vertex_count, remap_table, mesh.attrib_buffer
            );
        }
        else
        {
            indexed_vertex_count = static_cast<uint32_t>(meshopt_generateVertexRemap(
                remap_table.data(), nullptr, mesh.element_count, streams[0].data, mesh.element_count, streams[0].size
            ));
        }

        void* vertex_positions = nullptr;
        update_persistent_vertex_buffer<VertexBufferT>(
            streams[0], *layouts[0], mesh.element_count, indexed_vertex_count, remap_table, mesh.position_buffer, &vertex_positions
        );

        update_persistent_index_buffer<IndexBufferT>(
            mesh.element_count,
            indexed_vertex_count,
            remap_table,
            (mesh.flags & PRIMITIVE_TYPE_MASK) <= PRIMITIVE_QUADS,
            static_cast<float*>(vertex_positions),
            mesh.index_buffer
        );
    }

    template <typename BufferT>
    inline static void update_persistent_vertex_buffer
    (
        const meshopt_Stream&       stream,
        const bgfx::VertexLayout&   layout,
        uint32_t                    vertex_count,
        uint32_t                    indexed_vertex_count,
        const Vector<unsigned int>& remap_table,
        uint16_t&                   dst_buffer_handle,
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
            dst_buffer_handle = bgfx::createVertexBuffer(memory, layout).idx;
        }

        if constexpr (std::is_same<BufferT, bgfx::DynamicVertexBufferHandle>::value)
        {
            dst_buffer_handle = bgfx::createDynamicVertexBuffer(memory, layout).idx;
        }

        ASSERT(dst_buffer_handle != bgfx::kInvalidHandle);
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
        uint16_t&                   dst_buffer_handle
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
            dst_buffer_handle = bgfx::createIndexBuffer(memory, buffer_flags).idx;
        }

        if constexpr (std::is_same<BufferT, bgfx::DynamicIndexBufferHandle>::value)
        {
            dst_buffer_handle = bgfx::createDynamicIndexBuffer(memory, buffer_flags).idx;
        }

        ASSERT(dst_buffer_handle != bgfx::kInvalidHandle);
    }

private:
    Mutex                               m_mutex;
    Array<Mesh, MAX_MESHES>             m_meshes;
    Vector<uint16_t>                    m_transient_mesh_idxs;
    Vector<bgfx::TransientVertexBuffer> m_transient_vertex_buffers;
};


// -----------------------------------------------------------------------------
// TRANSIENT BUFFERS
// -----------------------------------------------------------------------------

class TransientBuffers
{
public:
    inline bool update_from_recorder(const GeometryRecorder& recorder)
    {
        return
            update_buffer(recorder.position_buffer(), m_positions) &&
            update_buffer(recorder.attrib_buffer  (), m_attribs  );
    }

    inline const bgfx::TransientVertexBuffer* positions() const { return &m_positions; }

    inline const bgfx::TransientVertexBuffer* attribs() const { return &m_attribs; }

private:
    static bool update_buffer(const Vector<uint8_t>& src, bgfx::TransientVertexBuffer& dst)
    {
        if (!src.empty())
        {
            const uint32_t dummy_vertex_count = static_cast<uint32_t>(src.size() / ms_dummy_vertex_layout.getStride());

            if (bgfx::getAvailTransientVertexBuffer(dummy_vertex_count, ms_dummy_vertex_layout) < dummy_vertex_count)
            {
                ASSERT(false && "Unable to allocate requested number of transient vertices.");
                return false;
            }

            bgfx::allocTransientVertexBuffer(&dst, dummy_vertex_count, ms_dummy_vertex_layout);
            (void)memcpy(dst.data, src.data(), src.size());
        }
        else
        {
            dst = { nullptr, 0 };
        }

        return true;
    }

private:
    static const bgfx::VertexLayout ms_dummy_vertex_layout;

    bgfx::TransientVertexBuffer     m_positions = { nullptr, 0 };
    bgfx::TransientVertexBuffer     m_attribs   = { nullptr, 0 };
};

const bgfx::VertexLayout TransientBuffers::ms_dummy_vertex_layout = []()
{
    bgfx::VertexLayout layout;

    layout
        .begin()
        .add  (bgfx::Attrib::TexCoord7, 1, bgfx::AttribType::Float)
        .end  ();

    ASSERT(layout.getStride() % 4 == 0);

    return layout;
}();


// -----------------------------------------------------------------------------
// GEOMETRY SUBMISSION
// -----------------------------------------------------------------------------

static void submit_draw_list
(
    const DrawList&          draw_list,
    const MeshCache&         mesh_cache,
    const VertexLayoutCache& vertex_layout_cache,
    bool                     is_main_thread
)
{
    bgfx::Encoder* encoder = bgfx::begin(!is_main_thread);
    if (!encoder)
    {
        ASSERT(false && "Failed to obtain BGFX encoder.");
        return;
    }

    bgfx::Transform transforms       = { nullptr, 0 };
    const uint32_t  transform_offset = encoder->allocTransform(&transforms, static_cast<uint16_t>(draw_list.matrices().size()));

    if (transforms.data)
    {
        memcpy(transforms.data, draw_list.matrices().data(), draw_list.matrices().size() * sizeof(Mat4));
    }

    static const uint64_t primitive_flags[] =
    {
        0, // Triangles.
        0, // Quads (for users, triangles internally).
        BGFX_STATE_PT_TRISTRIP,
        BGFX_STATE_PT_LINES,
        BGFX_STATE_PT_LINESTRIP,
    };

    for (const DrawItem& item : draw_list.items())
    {
        const Mesh& mesh = mesh_cache.mesh(item.mesh);

        switch (mesh.type())
        {
        case MESH_TRANSIENT:
                                encoder->setVertexBuffer(0, &mesh_cache.transient_vertex_buffers()[mesh.position_buffer]);
            if (mesh.attribs()) encoder->setVertexBuffer(1, &mesh_cache.transient_vertex_buffers()[mesh.attrib_buffer  ]);
            break;

        case MESH_STATIC:
                                encoder->setVertexBuffer(0, bgfx::VertexBufferHandle { mesh.position_buffer });
            if (mesh.attribs()) encoder->setVertexBuffer(1, bgfx::VertexBufferHandle { mesh.attrib_buffer   });
                                encoder->setIndexBuffer (   bgfx::IndexBufferHandle  { mesh.index_buffer    });
            break;

        case MESH_DYNAMIC:
                                encoder->setVertexBuffer(0, bgfx::DynamicVertexBufferHandle { mesh.position_buffer });
            if (mesh.attribs()) encoder->setVertexBuffer(1, bgfx::DynamicVertexBufferHandle { mesh.attrib_buffer   });
                                encoder->setIndexBuffer (   bgfx::DynamicIndexBufferHandle  { mesh.index_buffer    });
            break;

        default:
            ASSERT(false && "Invalid mesh type.");
            break;
        }

        if (bgfx::isValid(item.texture) && bgfx::isValid(item.sampler))
        {
            encoder->setTexture(0, item.sampler, item.texture);
        }

        encoder->setTransform(transform_offset + item.transform);

        encoder->setState(
            BGFX_STATE_DEFAULT |
            primitive_flags[(mesh.flags & PRIMITIVE_TYPE_MASK) >> PRIMITIVE_TYPE_SHIFT]
        );

        ASSERT(bgfx::isValid(item.program));
        encoder->submit(item.pass, item.program);
    }

    bgfx::end(encoder);
}


// -----------------------------------------------------------------------------
// TEXTURING
// -----------------------------------------------------------------------------

struct Texture
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    uint16_t            width  = 0;
    uint16_t            height = 0;

    void destroy()
    {
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

        // TODO : Move elsewhere?
        constexpr uint16_t TEXTURE_SAMPLING_MASK  = TEXTURE_NEAREST;
        constexpr uint16_t TEXTURE_SAMPLING_SHIFT = 0;

        constexpr uint16_t TEXTURE_BORDER_MASK    = TEXTURE_MIRROR | TEXTURE_CLAMP;
        constexpr uint16_t TEXTURE_BORDER_SHIFT   = 1;

        constexpr uint16_t TEXTURE_FORMAT_MASK    = TEXTURE_R8 | TEXTURE_D24S8 | TEXTURE_D32F;
        constexpr uint16_t TEXTURE_FORMAT_SHIFT   = 3;

        constexpr uint16_t TEXTURE_TARGET_MASK    = TEXTURE_TARGET;
        constexpr uint16_t TEXTURE_TARGET_SHIFT   = 6;

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
            0,
            BGFX_TEXTURE_RT, // TEXTURE_TARGET
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
                    memcpy(dst, src, width * format.size);

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

        texture.width  = width;
        texture.height = height;
    }

    inline const Texture& texture(uint16_t id) const { return m_textures[id]; }

private:
    Mutex                        m_mutex;
    Array<Texture, MAX_TEXTURES> m_textures;
};


// -----------------------------------------------------------------------------
// FRAMEBUFFERS
// -----------------------------------------------------------------------------

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

    void add_framebuffer(const FramebufferRecorder& recorder)
    {
        MutexScope lock(m_mutex);

        Framebuffer& framebuffer = m_framebuffers[recorder.id()];
        framebuffer.destroy();
        framebuffer = recorder.create_framebuffer();
    }

    inline const Framebuffer& framebuffer(uint16_t id) const { return m_framebuffers[id]; }

private:
    Mutex                                m_mutex;
    Array<Framebuffer, MAX_FRAMEBUFFERS> m_framebuffers;
};


// -----------------------------------------------------------------------------
// TIME MEASUREMENT
// -----------------------------------------------------------------------------

class Timer 
{
public:
    inline void tic()
    {
        m_counter = bx::getHPCounter();
    }

    double toc(bool restart = false)
    {
        const int64_t now = bx::getHPCounter();

        m_elapsed = (now - m_counter) / ms_frequency;

        if (restart)
        {
            m_counter = now;
        }

        return m_elapsed;
    }

    inline double elapsed() const { return m_elapsed; }

private:
    int64_t             m_counter = 0;
    double              m_elapsed = 0.0;

    static const double ms_frequency;
};

const double Timer::ms_frequency = static_cast<double>(bx::getHPFrequency());


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
        ASSERT(handle);

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
    ASSERT(func);
    (*func)(data);

    ASSERT(pool);
    pool->release_task(this);
}


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
    FramebufferCache    framebuffer_cache;
    ProgramCache        program_cache;
    TextureCache        texture_cache;
    DefaultUniforms     default_uniforms;
    VertexLayoutCache   vertex_layout_cache;
    bgfx::VertexLayout  dummy_vertex_layout;

    Window              window;

    Timer               total_time;
    Timer               frame_time;

    Atomic<uint32_t>    frame_number      = 0;

    bool                vsync_on          = false;
    bool                reset_back_buffer = true;
};

struct LocalContext
{
    GeometryRecorder    recorder;

    FramebufferRecorder framebuffer_recorder;

    DrawList            draw_list;
    TransientBuffers    transient_buffers;

    PassStack           pass_stack;

    MatrixStack         view_matrix_stack;
    MatrixStack         proj_matrix_stack;
    MatrixStack         model_matrix_stack;

    Timer               stop_watch;
    Timer               frame_time;

    MatrixStack*        active_matrix_stack = &model_matrix_stack;

    uint16_t            recorded_mesh_id    = UINT16_MAX;
    uint16_t            recorded_mesh_flags = 0;

    bool                is_recording        = false;
    bool                is_main_thread      = false;
};

static GlobalContext g_ctx;

// TODO : Number of these should probably be limited and the non-main thread
//        should explicitly ask for them and release them (possibly with the
//        exception of the lightweight items (timers, main-thread-ness flag, ...)).
thread_local LocalContext t_ctx;


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY (C++)
// -----------------------------------------------------------------------------

int run(void (* init)(void), void (*setup)(void), void (*draw)(void), void (*cleanup)(void))
{
    // TODO : Check we're not being called multiple times witohut first terminating.
    // TODO : Reset global context data (thread local as well, if possible, but might not be).
    // TODO : Add GLFW error callback and exit `mnm_run` if an error occurrs.

    t_ctx.is_main_thread = true;

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
        bgfx::Init init;
        init.platformData = create_platform_data(g_ctx.window.handle, init.type);

        if (!bgfx::init(init))
        {
            glfwDestroyWindow(g_ctx.window.handle);
            glfwTerminate();
            return 3;
        }
    }

    g_ctx.task_scheduler.Initialize(std::max(3u, std::thread::hardware_concurrency()) - 1);

    g_ctx.vertex_layout_cache.add_builtins();

    g_ctx.dummy_vertex_layout
        .begin()
        .add  (bgfx::Attrib::TexCoord7, 1, bgfx::AttribType::Float)
        .end  ();
    ASSERT(g_ctx.dummy_vertex_layout.getStride() % 4 == 0);

    if (setup)
    {
        (*setup)();
    }

    bgfx::setDebug(BGFX_DEBUG_STATS);

    const bgfx::RendererType::Enum    type        = bgfx::getRendererType();
    static const bgfx::EmbeddedShader s_shaders[] =
    {
        BGFX_EMBEDDED_SHADER(position_color_fs         ),
        BGFX_EMBEDDED_SHADER(position_color_vs         ),

        BGFX_EMBEDDED_SHADER(position_color_texcoord_fs),
        BGFX_EMBEDDED_SHADER(position_color_texcoord_vs),

        BGFX_EMBEDDED_SHADER(position_texcoord_fs      ),
        BGFX_EMBEDDED_SHADER(position_texcoord_vs      ),

        BGFX_EMBEDDED_SHADER_END()
    };

    {
        const struct
        {
            const char* name;
            uint16_t    attribs;
        }
        programs[] =
        {
            { "position_color"         , VERTEX_COLOR                   },
            { "position_color_texcoord", VERTEX_COLOR | VERTEX_TEXCOORD },
            { "position_texcoord"      ,                VERTEX_TEXCOORD },
        };

        char vs_name[32];
        char fs_name[32];

        for (int i = 0; i < BX_COUNTOF(programs); i++)
        {
            strcpy(vs_name, programs[i].name);
            strcat(vs_name, "_vs");

            strcpy(fs_name, programs[i].name);
            strcat(fs_name, "_fs");

            (void)g_ctx.program_cache.add(s_shaders, type, vs_name, fs_name, programs[i].attribs);
        }
    }

    g_ctx.default_uniforms.init();

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

        glfwPollEvents();

        bool update_cursor_position = false;

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
                update_cursor_position = true;
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

        if (g_ctx.reset_back_buffer)
        {
            g_ctx.reset_back_buffer = false;

            g_ctx.window.update_size_info();

            const uint16_t width  = static_cast<uint16_t>(g_ctx.window.framebuffer_width );
            const uint16_t height = static_cast<uint16_t>(g_ctx.window.framebuffer_height);

            const uint32_t vsync  = g_ctx.vsync_on ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

            bgfx::reset(width, height, BGFX_RESET_NONE | vsync);

            g_ctx.pass_cache[DEFAULT_PASS].set_viewport(0, 0, width, height);
        }

        if (update_cursor_position)
        {
            g_ctx.mouse.update_position(g_ctx.window);
        }

        g_ctx.mouse.update_position_delta();

        // We don't clear on zero-th frame, since the user may have recorded
        // something in the `setup` callback.
        if (g_ctx.frame_number > 0)
        {
            // TODO : This needs to be done for all contexts across all threads.
            t_ctx.draw_list.clear();
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (draw)
        {
            (*draw)();
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (t_ctx.is_main_thread)
        {
            // This means the default pass transforms are governed by the matrix stacks on the main thread.
            // We could avoid it by having the user explictly specify default pass delimiter, but it seems
            // fine like this, at least for now, and it's less typing in most cases. Or perhaps just add
            // explicit function like `default_pass` that would store the current matrices in both stacks
            // in whichever thread it would be called from.
            g_ctx.pass_cache.set_pass_transforms(
                DEFAULT_PASS,
                t_ctx.view_matrix_stack.top(),
                t_ctx.proj_matrix_stack.top()
            );

            g_ctx.pass_cache.update();
        }

        // TODO : This needs to be done for all contexts across all threads.
        submit_draw_list(t_ctx.draw_list, g_ctx.mesh_cache, g_ctx.vertex_layout_cache, t_ctx.is_main_thread);

        if (t_ctx.is_main_thread)
        {
            g_ctx.mesh_cache.clear_transient_meshes();
        }

        bgfx::frame();
        g_ctx.frame_number++;
    }

    if (cleanup)
    {
        (*cleanup)();
    }

    g_ctx.task_scheduler.WaitforAllAndShutdown();

    // TODO : Proper destruction of cached buffers and other framework-retained BGFX resources.
    g_ctx.vertex_layout_cache.clear();
    g_ctx.texture_cache      .clear();
    g_ctx.program_cache      .clear();
    g_ctx.default_uniforms   .clear();
    g_ctx.mesh_cache         .clear_persistent_meshes();

    bgfx::shutdown();

    glfwDestroyWindow(g_ctx.window.handle);
    glfwTerminate();

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
    ASSERT(mnm::t_ctx.is_main_thread);
    ASSERT(mnm::g_ctx.window.display_scale_x);
    ASSERT(mnm::g_ctx.window.display_scale_y);

    // TODO : Round instead?
    if (mnm::g_ctx.window.position_scale_x != 1.0f) { width  = static_cast<int>(width  * mnm::g_ctx.window.display_scale_x); }
    if (mnm::g_ctx.window.position_scale_y != 1.0f) { height = static_cast<int>(height * mnm::g_ctx.window.display_scale_y); }

    mnm::resize_window(mnm::g_ctx.window.handle, width, height, flags);
}

void title(const char* title)
{
    ASSERT(mnm::t_ctx.is_main_thread);

    glfwSetWindowTitle(mnm::g_ctx.window.handle, title);
}

void vsync(int vsync)
{
    ASSERT(mnm::t_ctx.is_main_thread);

    mnm::g_ctx.vsync_on          = static_cast<bool>(vsync);
    mnm::g_ctx.reset_back_buffer = true;
}

void quit(void)
{
    ASSERT(mnm::t_ctx.is_main_thread);

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

int key_down(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::DOWN);
}

int key_held(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::HELD);
}

int key_up(int key)
{
    return mnm::g_ctx.keyboard.is(key, mnm::Keyboard::UP);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TIME
// -----------------------------------------------------------------------------

double elapsed(void)
{
    return mnm::g_ctx.total_time.elapsed();
}

double dt(void)
{
    return mnm::g_ctx.frame_time.elapsed();
}

void sleep_for(double seconds)
{
    ASSERT(!mnm::t_ctx.is_main_thread);

    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

void tic(void)
{
    mnm::t_ctx.stop_watch.tic();
}

double toc(void)
{
    return mnm::t_ctx.stop_watch.toc();
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - GEOMETRY
// -----------------------------------------------------------------------------

static inline void begin_recording(int id, int flags, mnm::MeshType type)
{
    ASSERT(id > 0 && id < mnm::MAX_MESHES);

    ASSERT(!mnm::t_ctx.is_recording);

    mnm::t_ctx.recorded_mesh_id    = static_cast<uint16_t>(id);
    mnm::t_ctx.recorded_mesh_flags = static_cast<uint16_t>(flags | static_cast<int>(type));
    mnm::t_ctx.is_recording        = true;

    mnm::t_ctx.recorder.reset(mnm::t_ctx.recorded_mesh_flags);
}

void begin_transient(int id, int flags)
{
    begin_recording(id, flags, mnm::MESH_TRANSIENT);
}

void begin_static(int id, int flags)
{
    begin_recording(id, flags, mnm::MESH_STATIC);
}

void begin_dynamic(int id, int flags)
{
    begin_recording(id, flags, mnm::MESH_DYNAMIC);
}

void vertex(float x, float y, float z)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.recorder.vertex((mnm::t_ctx.model_matrix_stack.top() * HMM_Vec4(x, y, z, 1.0f)).XYZ);
}

void color(unsigned int rgba)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.recorder.color(rgba);
}

void normal(float nx, float ny, float nz)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.recorder.normal(nx, ny, nz);
}

void texcoord(float u, float v)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.recorder.texcoord(u, v);
}

void end(void)
{
    using namespace mnm;

    ASSERT(t_ctx.is_recording);

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)g_ctx.mesh_cache.add_mesh(t_ctx.recorded_mesh_id, t_ctx.recorded_mesh_flags, t_ctx.recorder, g_ctx.vertex_layout_cache);

    t_ctx.recorded_mesh_id    = UINT16_MAX;
    t_ctx.recorded_mesh_flags = 0;
    t_ctx.is_recording        = false;
}

void mesh(int id)
{
    using namespace mnm;

    ASSERT(id > 0 && id < MAX_MESHES);
    ASSERT(!t_ctx.is_recording);

    // TODO : This "split data filling" is silly, it should be done either fully
    //        here or in the draw list.
    DrawItem& state = t_ctx.draw_list.state();

    state.pass = t_ctx.pass_stack.top();

    state.framebuffer = g_ctx.pass_cache[t_ctx.pass_stack.top()].framebuffer();

    if (!bgfx::isValid(state.program))
    {
        state.program = g_ctx.program_cache.program_handle_from_flags(
            g_ctx.mesh_cache.mesh(static_cast<uint16_t>(id)).flags
        );
    }

    t_ctx.draw_list.submit_mesh(static_cast<uint16_t>(id), t_ctx.model_matrix_stack.top());
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURING
// -----------------------------------------------------------------------------

void load_texture(int id, int flags, int width, int height, int stride, const void* data)
{
    ASSERT(id > 0 && id < mnm::MAX_TEXTURES);
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
    load_texture(id, flags, width, height, 0, 0);
}

void texture(int id)
{
    using namespace mnm;

    ASSERT(id > 0 && id < MAX_TEXTURES);

    if (!t_ctx.framebuffer_recorder.is_recording())
    {
        // TODO : Samplers should be set by default state and only overwritten when
        //        non-default shader is used.
        DrawItem& state = t_ctx.draw_list.state();

        state.texture = g_ctx.texture_cache.texture(static_cast<uint16_t>(id)).handle;
        state.sampler = g_ctx.default_uniforms.color_texture;
    }
    else
    {
        t_ctx.framebuffer_recorder.add_texture(g_ctx.texture_cache.texture(static_cast<uint16_t>(id)));
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - PASSES
// -----------------------------------------------------------------------------

void begin_pass(int id)
{
    ASSERT(id > 0 && id < mnm::MAX_PASSES);

    mnm::t_ctx.pass_stack.top_and_push(static_cast<bgfx::ViewId>(id));

    mnm::t_ctx.view_matrix_stack.push();
    mnm::t_ctx.proj_matrix_stack.push();
}

void end_pass(void)
{
    using namespace mnm;

    t_ctx.pass_stack.pop();

    g_ctx.pass_cache.set_pass_transforms(
        t_ctx.pass_stack       .top(),
        t_ctx.view_matrix_stack.top(),
        t_ctx.proj_matrix_stack.top()
    );

    t_ctx.view_matrix_stack.pop();
    t_ctx.proj_matrix_stack.pop();
}

void no_clear(void)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx.pass_stack.top()].set_no_clear();
}

void clear_depth(float depth)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx.pass_stack.top()].set_clear_depth(depth);
}

void clear_color(unsigned int rgba)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx.pass_stack.top()].set_clear_color(rgba);
}

void viewport(int x, int y, int width, int height)
{
    ASSERT(x      >= 0);
    ASSERT(y      >= 0);
    ASSERT(width  >  0);
    ASSERT(height >  0);

    mnm::g_ctx.pass_cache[mnm::t_ctx.pass_stack.top()].set_viewport(
        static_cast<uint16_t>(x),
        static_cast<uint16_t>(y),
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height)
    );
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FRAMEBUFFERS
// -----------------------------------------------------------------------------

void begin_framebuffer(int id)
{
    ASSERT(id > 0 && id < mnm::MAX_FRAMEBUFFERS);

    mnm::t_ctx.framebuffer_recorder.begin(static_cast<uint16_t>(id));
}

void end_framebuffer(void)
{
    mnm::g_ctx.framebuffer_cache.add_framebuffer(mnm::t_ctx.framebuffer_recorder);
    mnm::t_ctx.framebuffer_recorder.end();
}

void no_framebuffer(void)
{
    mnm::g_ctx.pass_cache[mnm::t_ctx.pass_stack.top()].set_framebuffer({ BGFX_INVALID_HANDLE });
}

void framebuffer(int id)
{
    ASSERT(id > 0 && id < mnm::MAX_FRAMEBUFFERS);

    mnm::g_ctx.pass_cache[mnm::t_ctx.pass_stack.top()].set_framebuffer(
        mnm::g_ctx.framebuffer_cache.framebuffer(static_cast<uint16_t>(id))
    );
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TRANSFORMATIONS
// -----------------------------------------------------------------------------

void model(void)
{
    mnm::t_ctx.active_matrix_stack = &mnm::t_ctx.model_matrix_stack;
}

void view(void)
{
    mnm::t_ctx.active_matrix_stack = &mnm::t_ctx.view_matrix_stack;
}

void projection(void)
{
    mnm::t_ctx.active_matrix_stack = &mnm::t_ctx.proj_matrix_stack;
}

void push(void)
{
    ASSERT(mnm::t_ctx.active_matrix_stack);
    mnm::t_ctx.active_matrix_stack->push();
}

void pop(void)
{
    ASSERT(mnm::t_ctx.active_matrix_stack);
    mnm::t_ctx.active_matrix_stack->pop();
}

void identity(void)
{
    mnm::t_ctx.active_matrix_stack->top() = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near_, float far_)
{
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_Orthographic(left, right, bottom, top, near_, far_));
}

void perspective(float fovy, float aspect, float near_, float far_)
{
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_Perspective(fovy, aspect, near_, far_));
}

void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y, float at_z, float up_x, float up_y, float up_z)
{
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_LookAt(HMM_Vec3(eye_x, eye_y, eye_z), HMM_Vec3(at_x, at_y, at_z), HMM_Vec3(up_x, up_y, up_z)));
}

void rotate(float angle, float x, float y, float z)
{
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_Rotate(angle, HMM_Vec3(x, y, z)));
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
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_Scale(HMM_Vec3(scale, scale, scale)));
}

void translate(float x, float y, float z)
{
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_Translate(HMM_Vec3(x, y, z)));
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
// PUBLIC API IMPLEMENTATION - MISCELLANEOUS
// -----------------------------------------------------------------------------

int frame(void)
{
    return static_cast<int>(mnm::g_ctx.frame_number);
}
