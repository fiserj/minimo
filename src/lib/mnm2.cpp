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

#include <TaskScheduler.h>        // ITaskSet, TaskScheduler, TaskSetPartition

namespace mnm
{

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
    uint16_t add(const bgfx::VertexLayout& layout)
    {
        m_layouts.push_back(layout);
        m_handles.push_back(bgfx::createVertexLayout(layout));

        return static_cast<uint16_t>(m_handles.size() - 1);
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
    uint16_t vertex_layout_id;
};

class GeometryRecorder
{
private:
    using VertexPushFunc = void (*)(GeometryRecorder&, const Vec3&);

public:
    void begin(int user_id, uint32_t attribs, uint16_t vertex_layout_id)
    {
        ASSERT(!m_recording);
        ASSERT( user_id);
        ASSERT( attribs == (attribs & (VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD)));
        ASSERT( attribs <  BX_COUNTOF(ms_push_func_table));
        ASSERT( ms_push_func_table[attribs]);

        m_push_func = ms_push_func_table[attribs];

        GeometryRecord record;
        record.user_id          = user_id;
        record.attribs          = attribs;
        record.byte_offset      = static_cast<uint32_t>(m_buffer.size());
        record.byte_length      = 0;
        record.vertex_layout_id = vertex_layout_id;

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

} // namespace mnm


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION
// -----------------------------------------------------------------------------

// ...
