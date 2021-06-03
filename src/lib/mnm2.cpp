#include <mnm/mnm.h>

#include <assert.h>               // assert
#include <stddef.h>               // ptrdiff_t, size_t
#include <stdint.h>               // *int*_t, UINT*_MAX
#include <string.h>               // memcpy

#include <algorithm>              // max, transform
#include <atomic>                 // atomic
#include <array>                  // array
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

#include <meshoptimizer.h>        // meshopt_*

#include <TaskScheduler.h>        // ITaskSet, TaskScheduler, TaskSetPartition

#include <shaders/poscolor_fs.h>  // poscolor_fs
#include <shaders/poscolor_vs.h>  // poscolor_vs


namespace mnm
{

// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

enum MeshType : uint8_t
{
    MESH_INVALID   = 0,
    MESH_TRANSIENT = 1,
    MESH_STATIC    = 2,
    MESH_DYNAMIC   = 3,
};

enum
{
    VERTEX_POSITION = 0,
};

constexpr uint32_t VERTEX_ATTRIB_SHIFT   = 0;

constexpr uint32_t VERTEX_ATTRIB_MASK    = (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD) << VERTEX_ATTRIB_SHIFT;

constexpr uint32_t MESH_TYPE_SHIFT       = 3;

constexpr uint32_t MESH_TYPE_MASK        = (MESH_TRANSIENT | MESH_STATIC) << MESH_TYPE_SHIFT;

constexpr uint32_t MAX_MESHES            = 4096;

constexpr uint32_t MAX_TASKS             = 64;

constexpr uint16_t MIN_WINDOW_SIZE       = 240;

constexpr uint16_t DEFAULT_WINDOW_WIDTH  = 800;

constexpr uint16_t DEFAULT_WINDOW_HEIGHT = 600;


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

template <typename KeyT, typename T>
using HashMap = std::unordered_map<KeyT, T>;

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


// -----------------------------------------------------------------------------
// DRAW SUBMISSION
// -----------------------------------------------------------------------------

struct DrawItem
{
    // TODO : Mesh ID and view ID could be merged to single uint16_t.
    uint16_t                 transform = UINT16_MAX;
    uint16_t                 mesh      = UINT16_MAX;
    bgfx::ViewId             pass      = UINT16_MAX;
    bgfx::ProgramHandle      program   = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle      texture   = BGFX_INVALID_HANDLE;
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
    // NOTE : attribs here aren't shifted!
    uint8_t add(bgfx::ShaderHandle vertex, bgfx::ShaderHandle fragment, uint32_t attribs = UINT32_MAX)
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

        if (attribs != UINT32_MAX)
        {
            ASSERT(attribs == (attribs & (VERTEX_ATTRIB_MASK >> VERTEX_ATTRIB_SHIFT)));
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
        uint32_t                    attribs = UINT32_MAX
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

    inline bgfx::ProgramHandle program_handle_from_flags(uint32_t flags) const
    {
        const uint32_t attribs = (flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT;

        ASSERT(attribs < m_attribs_to_ids.size());
        ASSERT(m_attribs_to_ids[attribs] != UINT8_MAX);

        return program_handle_from_id(m_attribs_to_ids[attribs]);
    }

private:
    Vector<bgfx::ProgramHandle> m_handles;
    Vector<uint8_t>             m_attribs_to_ids;
};


// -----------------------------------------------------------------------------
// VERTEX LAYOUT CACHE
// -----------------------------------------------------------------------------

class VertexLayoutCache
{
public:
    void add(uint32_t attribs)
    {
        attribs = (attribs & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT;

        if (attribs < m_handles.size() && bgfx::isValid(m_handles[attribs]))
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
        ASSERT(layout.getStride() % 4 == 0);

        if (attribs >= m_layouts.size())
        {
            m_layouts.resize(attribs + 1);
            m_handles.resize(attribs + 1, BGFX_INVALID_HANDLE);
        }

        m_layouts[attribs] = layout;
        m_handles[attribs] = bgfx::createVertexLayout(layout);

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

    inline const bgfx::VertexLayout& layout(uint32_t attribs) const
    {
        ASSERT(((attribs & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT) < m_layouts.size());

        return m_layouts[((attribs & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT)];
    }

    inline bgfx::VertexLayoutHandle handle(uint32_t attribs) const
    {
        ASSERT(((attribs & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT) < m_handles.size());

        return m_handles[((attribs & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT)];
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

struct GeometryRecord
{
    uint16_t mesh_id;
    uint16_t mesh_flags;
    uint32_t position_byte_offset;
    uint32_t attribs_byte_offset;
    uint32_t vertex_count;

    inline MeshType mesh_type() const
    {
        return static_cast<MeshType>((mesh_flags & MESH_TYPE_MASK) >> MESH_TYPE_SHIFT);
    }

    inline uint16_t mesh_attribs() const
    {
        return (mesh_flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT;
    }
};

class GeometryRecorder
{
private:
    using VertexPushFunc = void (*)(GeometryRecorder&, const Vec3&);

public:
    inline void clear()
    {
        ASSERT(!m_recording);

        m_records        .clear();
        m_attrib_buffer  .clear();
        m_position_buffer.clear();
    }

    void begin(uint16_t mesh_id, uint16_t mesh_flags, uint32_t alias_padding)
    {
        ASSERT(!m_recording);
        ASSERT(ms_push_func_table[(mesh_flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT]);
        ASSERT(alias_padding <= 128);

        m_push_func = ms_push_func_table[(mesh_flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT];

        if (alias_padding)
        {
            m_attrib_buffer.resize(m_attrib_buffer.size() + alias_padding, 0);
        }

        GeometryRecord record;
        record.mesh_id              = mesh_id;
        record.mesh_flags           = mesh_flags;
        record.attribs_byte_offset  = static_cast<uint32_t>(m_attrib_buffer  .size());
        record.position_byte_offset = static_cast<uint32_t>(m_position_buffer.size());
        record.vertex_count         = 0;

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

    inline const Vector<GeometryRecord>& records() const { return m_records; }

    inline const Vector<uint8_t>& attrib_buffer() const { return m_attrib_buffer; }

    inline const Vector<uint8_t>& position_buffer() const { return m_position_buffer; }

private:
    template <size_t Size>
    class DataPush
    {
    public:
        inline DataPush(Vector<uint8_t>& buffer)
        {
            reserve(buffer, Size);
        }

        template <typename T>
        inline DataPush(Vector<uint8_t>& buffer, const T& data)
        {
            static_assert(sizeof(T) == Size, "Pushed data size mismatch.");

            reserve(buffer, sizeof(T));
            (*this)(data);
        }

        template <typename T>
        inline void operator()(const T& data)
        {
            static_assert(is_pod<T>(), "Pushed data type is not POD.");
            static_assert(std::alignment_of<T>::value == 4, "Non-standard pushed data alignment.");

#ifndef NDEBUG
            ASSERT(m_push_bytes >= sizeof(T));
            m_push_bytes -= sizeof(T);
#endif

            *reinterpret_cast<T*>(m_head) = data;
            m_head += sizeof(T);
        }

    private:
        inline void reserve(Vector<uint8_t>& buffer, size_t push_bytes)
        {
            ASSERT(push_bytes > 0);

            const size_t size = buffer.size();

            buffer.resize(size + push_bytes);

            m_head       = buffer.data() + size;
#ifndef NDEBUG
            m_push_bytes = push_bytes;
#endif
        }

    private:
        uint8_t* m_head;
#ifndef NDEBUG
        size_t   m_push_bytes;
#endif
    };

    struct VertexAttribs
    {
        uint32_t color    = 0xffffffff;
        uint32_t texcoord = 0x00000000;
        uint32_t normal   = 0x00ff0000;
    };

    template <uint32_t Attribs>
    static constexpr size_t attribs_size()
    {
        size_t size = 0;

        if constexpr  (!!(Attribs & VERTEX_COLOR))
        {
            size += sizeof(VertexAttribs::color);
        }

        if constexpr  (!!(Attribs & VERTEX_NORMAL))
        {
            size += sizeof(VertexAttribs::normal);
        }

        if constexpr (!!(Attribs & VERTEX_TEXCOORD))
        {
            size += sizeof(VertexAttribs::texcoord);
        }

        return size;
    }

    template <uint32_t Attribs, size_t Size = attribs_size<Attribs>()>
    static void push_vertex(GeometryRecorder& recorder, const Vec3& position)
    {
        recorder.m_records.back().vertex_count++;

        DataPush<sizeof(Vec3)>(recorder.m_position_buffer, position);

        if constexpr (Size > 0)
        {
            DataPush<Size> push_data(recorder.m_attrib_buffer);

            if constexpr (!!(Attribs & VERTEX_COLOR))
            {
                push_data(recorder.m_attribs.color);
            }

            if constexpr (!!(Attribs & VERTEX_NORMAL))
            {
                push_data(recorder.m_attribs.normal);
            }

            if constexpr (!!(Attribs & VERTEX_TEXCOORD))
            {
                push_data(recorder.m_attribs.texcoord);
            }
        }
    }

protected:
    VertexAttribs          m_attribs;
    Vector<GeometryRecord> m_records;
    Vector<uint8_t>        m_attrib_buffer;
    Vector<uint8_t>        m_position_buffer;
    VertexPushFunc         m_push_func = nullptr;
    bool                   m_recording = false;

    static const VertexPushFunc ms_push_func_table[8];
};

const GeometryRecorder::VertexPushFunc GeometryRecorder::ms_push_func_table[8] =
{
    nullptr,

    GeometryRecorder::push_vertex<VERTEX_COLOR   >,
    GeometryRecorder::push_vertex<VERTEX_NORMAL  >,
    GeometryRecorder::push_vertex<VERTEX_TEXCOORD>,
        
    GeometryRecorder::push_vertex<VERTEX_COLOR  | VERTEX_NORMAL  >,
    GeometryRecorder::push_vertex<VERTEX_COLOR  | VERTEX_TEXCOORD>,
    GeometryRecorder::push_vertex<VERTEX_NORMAL | VERTEX_TEXCOORD>,

    GeometryRecorder::push_vertex<VERTEX_COLOR  | VERTEX_NORMAL | VERTEX_TEXCOORD>,
};


// -----------------------------------------------------------------------------
// BUFFERS
// -----------------------------------------------------------------------------

enum BufferType : uint16_t
{
    STATIC_VERTEX_BUFFER,
    STATIC_INDEX_BUFFER,
    DYNAMIC_VERTEX_BUFFER,
    DYNAMIC_INDEX_BUFFER,
};

template <typename HandleT>
struct BufferTypeEnum;

template <> struct BufferTypeEnum<bgfx::VertexBufferHandle       > { static constexpr BufferType VALUE = BufferType::STATIC_VERTEX_BUFFER ; };
template <> struct BufferTypeEnum<bgfx::IndexBufferHandle        > { static constexpr BufferType VALUE = BufferType::STATIC_INDEX_BUFFER  ; };
template <> struct BufferTypeEnum<bgfx::DynamicVertexBufferHandle> { static constexpr BufferType VALUE = BufferType::DYNAMIC_VERTEX_BUFFER; };
template <> struct BufferTypeEnum<bgfx::DynamicIndexBufferHandle > { static constexpr BufferType VALUE = BufferType::DYNAMIC_INDEX_BUFFER ; };

union VertexBufferUnion
{
    uint16_t                        handle_index;
    bgfx::VertexBufferHandle        static_buffer;
    bgfx::DynamicVertexBufferHandle dynamic_buffer;
};

union IndexBufferUnion
{
    uint16_t                        handle_index;
    bgfx::IndexBufferHandle         static_buffer;
    bgfx::DynamicIndexBufferHandle  dynamic_buffer;
};

union BufferUnion
{
    uint16_t                        handle_index;
    VertexBufferUnion               vertices;
    IndexBufferUnion                indices;
};


// -----------------------------------------------------------------------------
// MESH
// -----------------------------------------------------------------------------

struct TransientMesh
{
    uint16_t flags;
    uint32_t vertex_count;
    uint32_t positions_start_vertex;
    uint32_t attribs_start_vertex;
};

struct PersistentMesh
{
    uint16_t          flags;
    uint32_t          index_count;
    VertexBufferUnion positions;
    VertexBufferUnion attribs;
    IndexBufferUnion  indices;
};

static_assert(offsetof(TransientMesh, flags) == offsetof(PersistentMesh, flags),
    "Inconsistent transient and persistent mesh \"flags\" member location.");

static_assert(offsetof(TransientMesh, vertex_count) == offsetof(PersistentMesh, index_count),
    "Inconsistent transient and persistent mesh element count member location.");

union Mesh
{
    TransientMesh  transient;
    PersistentMesh persistent;

    struct
    {
        uint16_t   flags;
        uint32_t   element_count;
    };

    inline MeshType type() const
    {
        return static_cast<MeshType>((flags & MESH_TYPE_MASK) >> MESH_TYPE_SHIFT);
    }

    inline uint16_t attribs() const
    {
        return (flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT;
    }
};

static_assert(offsetof(Mesh, flags) == offsetof(PersistentMesh, flags),
    "Inconsistent mesh union \"flags\" member location.");

static_assert(offsetof(Mesh, element_count) == offsetof(PersistentMesh, index_count),
    "Inconsistent mesh union element count member location.");


// -----------------------------------------------------------------------------
// MESH CACHE
// -----------------------------------------------------------------------------

class BufferDeletionQueue
{
public:
    void execute()
    {
        for (Record record : m_records)
        {
            ASSERT(record.data.handle_index != bgfx::kInvalidHandle);

            switch (record.type)
            {
            case BufferType::STATIC_VERTEX_BUFFER:
                bgfx::destroy(record.data.vertices.static_buffer );
                break;
            case BufferType::STATIC_INDEX_BUFFER:
                bgfx::destroy(record.data.indices .static_buffer );
                break;
            case BufferType::DYNAMIC_VERTEX_BUFFER:
                bgfx::destroy(record.data.vertices.dynamic_buffer);
                break;
            case BufferType::DYNAMIC_INDEX_BUFFER:
                bgfx::destroy(record.data.indices .dynamic_buffer);
                break;
            }
        }

        m_records.clear();
    }

    template <typename HandleT>
    inline void enqueue(HandleT& handle)
    {
        if (bgfx::isValid(handle))
        {
            m_records.push_back({ BufferTypeEnum<HandleT>::VALUE, handle.idx });

            handle = BGFX_INVALID_HANDLE;
        }
    }

private:
    struct Record
    {
        BufferType  type;
        BufferUnion data;
    };

private:
    Vector<Record> m_records;
};

struct MeshCache
{
public:
    MeshCache()
    {
        memset(m_meshes.data(), 0, m_meshes.size() * sizeof(m_meshes[0]));
    }

    bool register_mesh(const GeometryRecord& record, const VertexLayoutCache& vertex_layout_cache)
    {
        ASSERT(record.mesh_id < m_meshes.size());

        MutexScope lock(m_mutex);

        Mesh& mesh = m_meshes[record.mesh_id];

        const MeshType old_type =        mesh.type();
        const MeshType new_type = record.mesh_type();

        if (new_type == MESH_INVALID)
        {
            ASSERT(false && "Invalid registered mesh type.");
            return false;
        }

        switch (old_type)
        {
        case MESH_TRANSIENT:
            ASSERT(false && "Forgot to call `unregister_transient_meshes` at the end of last frame?");
            break;

        case MESH_STATIC:
            m_buffer_deletion_queue.enqueue(mesh.persistent.positions.static_buffer);
            m_buffer_deletion_queue.enqueue(mesh.persistent.attribs  .static_buffer);
            m_buffer_deletion_queue.enqueue(mesh.persistent.indices  .static_buffer);
            break;

        case MESH_DYNAMIC:
            // TODO : Enable dynamic meshes' updating.
            m_buffer_deletion_queue.enqueue(mesh.persistent.positions.dynamic_buffer);
            m_buffer_deletion_queue.enqueue(mesh.persistent.attribs  .dynamic_buffer);
            m_buffer_deletion_queue.enqueue(mesh.persistent.indices  .dynamic_buffer);
            break;

        default:
            break;
        }
        
        mesh.flags         = record.mesh_flags;
        mesh.element_count = record.vertex_count;

        if (new_type == MESH_TRANSIENT)
        {
            constexpr uint32_t position_size = sizeof(Vec3);
            const     uint32_t attribs_size  = vertex_layout_cache.layout(mesh.attribs()).getStride();

            ASSERT(record.position_byte_offset % position_size == 0);
            ASSERT(record.attribs_byte_offset  % attribs_size  == 0);

            mesh.transient.positions_start_vertex = record.position_byte_offset / position_size;
            mesh.transient.attribs_start_vertex   = record.attribs_byte_offset  / attribs_size ;

            m_transient_mesh_idxs.push_back(record.mesh_id);
        }

        return true;
    }

    void unregister_transient_meshes()
    {
        MutexScope lock(m_mutex);

        for (uint16_t idx : m_transient_mesh_idxs)
        {
            ASSERT(m_meshes[idx].type() == MESH_TRANSIENT);

            m_meshes[idx].flags         =
            m_meshes[idx].element_count = 0;
        }

        m_transient_mesh_idxs.clear();
    }

    void clear_persistent_meshes()
    {
        MutexScope lock(m_mutex);

        for (Mesh& mesh : m_meshes)
        {
            switch (mesh.type())
            {
            case MESH_STATIC:
                m_buffer_deletion_queue.enqueue(mesh.persistent.positions.static_buffer);
                m_buffer_deletion_queue.enqueue(mesh.persistent.attribs  .static_buffer);
                m_buffer_deletion_queue.enqueue(mesh.persistent.indices  .static_buffer);
                break;

            case MESH_DYNAMIC:
                m_buffer_deletion_queue.enqueue(mesh.persistent.positions.dynamic_buffer);
                m_buffer_deletion_queue.enqueue(mesh.persistent.attribs  .dynamic_buffer);
                m_buffer_deletion_queue.enqueue(mesh.persistent.indices  .dynamic_buffer);
                break;

            default:
                break;
            }
        }

        m_buffer_deletion_queue.execute();
    }

    inline void clear_abandoned_persistent_buffers()
    {
        MutexScope lock(m_mutex);

        m_buffer_deletion_queue.execute();
    }

    void update_persistent_meshes
    (
        const GeometryRecorder&    recorder,
        const VertexLayoutCache&   vertex_layout_cache,
        std::vector<unsigned int>* aux_remap_table = nullptr
    )
    {
        // Note that it should be safe to not use mutexes here, since the
        // mesh recording is thread-local (and assuming that mesh with the
        // same ID wasn't recorder in multiple threads (we should eventually
        // try to check that)). But given that persistent meshes shouldn't
        // be updated that often, it's probably fine to do it here.

        std::vector<unsigned int>  local_remap_table;
        std::vector<unsigned int>* remap_table = aux_remap_table ? aux_remap_table : &local_remap_table;

        for (const GeometryRecord& record : recorder.records())
        {
            ASSERT(record.mesh_id < m_meshes.size());

            Mesh mesh;
            {
                MutexScope lock(m_mutex);
                mesh = m_meshes[record.mesh_id];
            }

            ASSERT(mesh.type()        == MESH_STATIC ||
                   mesh.type()        == MESH_DYNAMIC);
            ASSERT(mesh.flags         == record.mesh_flags  );
            ASSERT(mesh.element_count == record.vertex_count);

            // TODO : Relax this to static meshes when updating dynamic meshes is permitted, and only update is to be done.
            ASSERT(mesh.persistent.positions.handle_index == bgfx::kInvalidHandle);
            ASSERT(mesh.persistent.attribs  .handle_index == bgfx::kInvalidHandle);
            ASSERT(mesh.persistent.indices  .handle_index == bgfx::kInvalidHandle);

            {
                constexpr size_t position_size = sizeof(Vec3);
                const     size_t attribs_size  = vertex_layout_cache.layout(mesh.attribs()).getStride();

                const meshopt_Stream vertex_streams[2] =
                {
                    { recorder.position_buffer().data() + record.position_byte_offset, position_size, position_size },
                    { recorder.attrib_buffer  ().data() + record.attribs_byte_offset , attribs_size , attribs_size  },
                };

                remap_table->resize(mesh.element_count);

                const size_t indexed_vertex_count = meshopt_generateVertexRemapMulti(
                    remap_table->data(),
                    nullptr,
                    mesh.element_count,
                    mesh.element_count,
                    vertex_streams,
                    BX_COUNTOF(vertex_streams)
                );
                ASSERT(indexed_vertex_count < UINT32_MAX);

                uint16_t index_buffer_flags = BGFX_BUFFER_NONE;
                uint32_t index_type_size    = sizeof(uint16_t);

                if (indexed_vertex_count > UINT16_MAX)
                {
                    index_buffer_flags = BGFX_BUFFER_INDEX32;
                    index_type_size    = sizeof(uint32_t);
                }

                // TODO : We should allocate just a single memory block and point BGFX
                //        to individual parts without calling explicit allocations for
                //        single meshes (but need to check how the `ReleaseFn` works).
                //        Even better, we could use double-buffered recorders or some
                //        kind of frame allocators and don't bother with copying.

                const bgfx::Memory* indices = bgfx::alloc(record.vertex_count * index_type_size);
                ASSERT(indices && indices->data);

                if (index_type_size == sizeof(uint16_t))
                {
                    meshopt_remapIndexBuffer<uint16_t>(
                        reinterpret_cast<uint16_t*>(indices->data),
                        nullptr,
                        mesh.element_count,
                        remap_table->data()
                    );
                }
                else
                {
                    meshopt_remapIndexBuffer<uint32_t>(
                        reinterpret_cast<uint32_t*>(indices->data),
                        nullptr,
                        mesh.element_count,
                        remap_table->data()
                    );
                }

                const bgfx::Memory* positions = bgfx::alloc(static_cast<uint32_t>(indexed_vertex_count * vertex_streams[0].size));
                ASSERT(positions && positions->data);

                meshopt_remapVertexBuffer(
                    positions->data,
                    vertex_streams[0].data,
                    mesh.element_count,
                    vertex_streams[0].size,
                    remap_table->data()
                );

                const bgfx::Memory* attribs = nullptr;

                if ((mesh.attribs() & VERTEX_ATTRIB_MASK))
                {
                    attribs = bgfx::alloc(static_cast<uint32_t>(indexed_vertex_count * vertex_streams[1].size));
                    ASSERT(attribs && attribs->data);

                    meshopt_remapVertexBuffer(
                        attribs->data,
                        vertex_streams[1].data,
                        mesh.element_count,
                        vertex_streams[1].size,
                        remap_table->data()
                    );
                }

                const bgfx::VertexLayout& positions_layout = vertex_layout_cache.layout(VERTEX_POSITION << VERTEX_ATTRIB_SHIFT);
                const bgfx::VertexLayout& attribs_layout   = vertex_layout_cache.layout(mesh.attribs());

                if (mesh.type() == MESH_STATIC)
                {
                                 mesh.persistent.positions.static_buffer = bgfx::createVertexBuffer(positions, positions_layout  );
                    if (attribs) mesh.persistent.attribs  .static_buffer = bgfx::createVertexBuffer(positions, attribs_layout    );
                                 mesh.persistent.indices  .static_buffer = bgfx::createIndexBuffer (indices  , index_buffer_flags);
                }
                else
                {
                    // TODO : Probably needs resizeable flags.
                                 mesh.persistent.positions.dynamic_buffer = bgfx::createDynamicVertexBuffer(positions, positions_layout  );
                    if (attribs) mesh.persistent.attribs  .dynamic_buffer = bgfx::createDynamicVertexBuffer(positions, attribs_layout    );
                                 mesh.persistent.indices  .dynamic_buffer = bgfx::createDynamicIndexBuffer (indices  , index_buffer_flags);
                }

                ASSERT(mesh.persistent.positions.handle_index != bgfx::kInvalidHandle);
                ASSERT(mesh.persistent.attribs  .handle_index != bgfx::kInvalidHandle || ! attribs);
                ASSERT(mesh.persistent.indices  .handle_index != bgfx::kInvalidHandle);
            }

            {
                MutexScope lock(m_mutex);
                m_meshes[record.mesh_id] = mesh;
            }
        }
    }

    inline Mesh& mesh(uint16_t id) { return m_meshes[id]; }

    inline const Mesh& mesh(uint16_t id) const { return m_meshes[id]; }

private:
    Mutex                   m_mutex;
    Array<Mesh, MAX_MESHES> m_meshes;
    Vector<uint16_t>        m_transient_mesh_idxs;
    BufferDeletionQueue     m_buffer_deletion_queue;
};


// -----------------------------------------------------------------------------
// GEOMETRY UPDATE
// -----------------------------------------------------------------------------

static bool update_transient_geometry
(
    const GeometryRecorder&      recorder,
    const bgfx::VertexLayout&    dummy_vertex_layout,
    // TransientMesh& out_transient_mesh
    bgfx::TransientVertexBuffer& out_vertex_buffer
)
{
    /*if (recorder.buffer().size() % dummy_vertex_layout.getStride() != 0)
    {
        // TODO : If this happens regularly (it won't with built-in types, but
        //        might when/if we add custom ones), we should just pad the buffer.
        ASSERT(false && "Incompatible transient vertex buffer and vertex layout sizes.");
        return false;
    }

    const uint32_t dummy_vertex_count = static_cast<uint32_t>(recorder.buffer().size() / dummy_vertex_layout.getStride());

    if (bgfx::getAvailTransientVertexBuffer(dummy_vertex_count, dummy_vertex_layout) < dummy_vertex_count)
    {
        ASSERT(false && "Unable to allocate requested number of transient vertices.");
        return false;
    }

    bgfx::allocTransientVertexBuffer(&out_vertex_buffer, dummy_vertex_count, dummy_vertex_layout);

    (void)memcpy(out_vertex_buffer.data, recorder.buffer().data(), recorder.buffer().size());*/

    return true;
}


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
    const TransientBuffers&  transient_buffers,
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
    const uint32_t  transform_offset = bgfx::allocTransform(&transforms, static_cast<uint16_t>(draw_list.matrices().size()));

    if (transforms.data)
    {
        memcpy(transforms.data, draw_list.matrices().data(), draw_list.matrices().size() * sizeof(Mat4));
    }

    for (const DrawItem& item : draw_list.items())
    {
        const Mesh& mesh = mesh_cache.mesh(item.mesh);

        switch (mesh.type())
        {
        case MESH_TRANSIENT:
                                  bgfx::setVertexBuffer(0, transient_buffers.positions(), mesh.transient.positions_start_vertex, mesh.element_count, vertex_layout_cache.handle(VERTEX_POSITION));
            if (mesh.attribs()) { bgfx::setVertexBuffer(1, transient_buffers.attribs  (), mesh.transient.attribs_start_vertex  , mesh.element_count, vertex_layout_cache.handle(mesh.attribs() )); }
            break;

        case MESH_STATIC:
                                  bgfx::setVertexBuffer(0, mesh.persistent.positions.static_buffer);
            if (mesh.attribs()) { bgfx::setVertexBuffer(1, mesh.persistent.attribs  .static_buffer); }
                                  bgfx::setIndexBuffer (   mesh.persistent.indices  .static_buffer);
            break;

        case MESH_DYNAMIC:
                                  bgfx::setVertexBuffer(0, mesh.persistent.positions.dynamic_buffer);
            if (mesh.attribs()) { bgfx::setVertexBuffer(1, mesh.persistent.attribs  .dynamic_buffer); }
                                  bgfx::setIndexBuffer (   mesh.persistent.indices  .dynamic_buffer);
            break;

        default:
            ASSERT(false && "Invalid mesh type.");
            break;
        }

        // if (bgfx::isValid(item.texture))
        // {
        //     ASSERT(false && "Not implemented yet.");
        //     encoder->setTexture(0, BGFX_INVALID_HANDLE /* TODO : Sampler! */, item.texture);
        // }

        bgfx::setTransform(transform_offset + item.transform);

        ASSERT(bgfx::isValid(item.program));
        encoder->submit(item.pass, item.program);
    }

    bgfx::end(encoder);
}


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

    MeshCache           mesh_cache;
    ProgramCache        program_cache;
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
    GeometryRecorder            transient_recorder;
    GeometryRecorder            persistent_recorder;

    DrawList                    draw_list;
    TransientBuffers            transient_buffers;

    MatrixStack                 view_matrix_stack;
    MatrixStack                 proj_matrix_stack;
    MatrixStack                 model_matrix_stack;

    Timer                       stop_watch;
    Timer                       frame_time;

    GeometryRecorder*           active_recorder     = &transient_recorder;
    MatrixStack*                active_matrix_stack = &model_matrix_stack;

    bool                        is_recording        = false;
    bool                        is_main_thread      = false;
};

static GlobalContext g_ctx;

// TODO : Number of these should probably be limited and the non-main thread
//        should explicitly ask for them and release them (possibly with the
//        exception of the lightweight items (timers, main-thread-ness flag, ...)).
thread_local LocalContext t_ctx;


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY (C++)
// -----------------------------------------------------------------------------

int run(void (*setup)(void), void (*draw)(void), void (*cleanup)(void))
{
    // TODO : Check we're not being called multiple times witohut first terminating.
    // TODO : Reset global context data (thread local as well, if possible, but might not be).
    // TODO : Add GLFW error callback and exit `mnm_run` if an error occurrs.

    t_ctx.is_main_thread = true;

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

    bgfx::Init init;
    init.platformData = create_platform_data(g_ctx.window.handle, init.type);

    if (!bgfx::init(init))
    {
        glfwDestroyWindow(g_ctx.window.handle);
        glfwTerminate();
        return 3;
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

        //(void)update_cached_geometry(t_ctx.cached_recorder, g_ctx.layouts, t_ctx.cached_buffers);
    }

    bgfx::setDebug(BGFX_DEBUG_STATS);

    // TODO : The clear values should be exposable to the end-user.
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x333333ff);

    const bgfx::RendererType::Enum    type        = bgfx::getRendererType();
    static const bgfx::EmbeddedShader s_shaders[] =
    {
        BGFX_EMBEDDED_SHADER(poscolor_fs),
        BGFX_EMBEDDED_SHADER(poscolor_vs),

        BGFX_EMBEDDED_SHADER_END()
    };

    (void)g_ctx.program_cache.add(s_shaders, type, "poscolor_vs", "poscolor_fs", VERTEX_COLOR);

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
            bgfx::setViewRect(0, 0, 0, width, height);
        }

        if (update_cursor_position)
        {
            g_ctx.mouse.update_position(g_ctx.window);
        }

        g_ctx.mouse.update_position_delta();

        bgfx::touch(0);

        // We don't clear on zero-th frame, since the user may have recorded
        // something in the `setup` callback.
        if (g_ctx.frame_number > 0)
        {
            // TODO : This needs to be done for all contexts across all threads.
            t_ctx.transient_recorder .clear();
            t_ctx.persistent_recorder.clear();
            t_ctx.draw_list          .clear();
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (draw)
        {
            (*draw)();
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        // TODO : This has to be recorder for all the views (once the begin/end pass API is added).
        bgfx::setViewTransform(0, &t_ctx.view_matrix_stack.top(), &t_ctx.proj_matrix_stack.top());

        // TODO : This needs to be done for all contexts across all threads.
        {
            // TODO : We should check the return value.
            (void)t_ctx.transient_buffers.update_from_recorder(t_ctx.transient_recorder);

            (void)g_ctx.mesh_cache.update_persistent_meshes(t_ctx.persistent_recorder, g_ctx.vertex_layout_cache);

            submit_draw_list(t_ctx.draw_list, g_ctx.mesh_cache, t_ctx.transient_buffers, g_ctx.vertex_layout_cache, t_ctx.is_main_thread);
        }

        if (t_ctx.is_main_thread)
        {
            g_ctx.mesh_cache.clear_abandoned_persistent_buffers();
            g_ctx.mesh_cache.unregister_transient_meshes();
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

int mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    return mnm::run(setup, draw, cleanup);
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

static void begin_recording
(
    mnm::GeometryRecorder& recorder,
    int                    id,
    int                    flags,
    uint32_t               alias_padding = 0
)
{
    ASSERT(id > 0 && id < mnm::MAX_MESHES);

    ASSERT(!mnm::t_ctx.is_recording);
    mnm::t_ctx.is_recording = true;

    mnm::t_ctx.active_recorder = &recorder;
    mnm::t_ctx.active_recorder->begin(static_cast<uint16_t>(id), static_cast<uint32_t>(flags), alias_padding);
}

void begin_transient(int id, int attribs)
{
    const uint32_t buffer_size = static_cast<uint32_t>(mnm::t_ctx.transient_recorder.attrib_buffer().size());

    const uint32_t layout_size = mnm::g_ctx.vertex_layout_cache.layout(static_cast<uint32_t>(attribs)).getStride();

    const uint32_t alignment   = buffer_size % layout_size;

    attribs  = (attribs << mnm::VERTEX_ATTRIB_SHIFT) & mnm::VERTEX_ATTRIB_MASK;
    attribs |= mnm::MESH_TRANSIENT << mnm::MESH_TYPE_SHIFT;

    begin_recording(mnm::t_ctx.transient_recorder, id, attribs, alignment ? (layout_size - alignment) : 0);
}

void begin_static(int id, int attribs)
{
    attribs  = (attribs << mnm::VERTEX_ATTRIB_SHIFT) & mnm::VERTEX_ATTRIB_MASK;
    attribs |= mnm::MESH_STATIC << mnm::MESH_TYPE_SHIFT;

    begin_recording(mnm::t_ctx.persistent_recorder, id, attribs);
}

void vertex(float x, float y, float z)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.active_recorder->vertex((mnm::t_ctx.model_matrix_stack.top() * HMM_Vec4(x, y, z, 1.0f)).XYZ);
}

void color(unsigned int rgba)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.active_recorder->color(rgba);
}

void normal(float nx, float ny, float nz)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.active_recorder->normal(HMM_Vec3(nx, ny, nz));
}

void texcoord(float u, float v)
{
    ASSERT(mnm::t_ctx.is_recording);
    mnm::t_ctx.active_recorder->texcoord(HMM_Vec2(u, v));
}

void end(void)
{
    ASSERT(mnm::t_ctx.is_recording);
    ASSERT(mnm::t_ctx.active_recorder);

    mnm::t_ctx.is_recording = false;

    // TODO : Figure out error handling - crash or just ignore the submission?
    (void)mnm::g_ctx.mesh_cache.register_mesh(mnm::t_ctx.active_recorder->records().back(), mnm::g_ctx.vertex_layout_cache);

    mnm::t_ctx.active_recorder->end();
    mnm::t_ctx.active_recorder = nullptr;
}

void mesh(int id)
{
    ASSERT(id > 0 && id < mnm::MAX_MESHES);
    ASSERT(!mnm::t_ctx.is_recording);

    // TODO : This "split data filling" is silly, it should be done either fully
    //        here or in the draw list.
    mnm::DrawItem& state = mnm::t_ctx.draw_list.state();

    if (state.pass == UINT16_MAX)
    {
        // TODO : Assign default pass ID.
        state.pass = 0;
    }

    if (!bgfx::isValid(state.program))
    {
        state.program = mnm::g_ctx.program_cache.program_handle_from_flags(
            mnm::g_ctx.mesh_cache.mesh(static_cast<uint16_t>(id)).flags
        );
    }

    mnm::t_ctx.draw_list.submit_mesh(static_cast<uint16_t>(id), mnm::t_ctx.model_matrix_stack.top());
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
    mnm::t_ctx.active_matrix_stack->multiply_top(HMM_Rotate(angle, HMM_Vec3(x, y, x)));
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


// -----------------------------------------------------------------------------
// !!! TEST
// -----------------------------------------------------------------------------

void TEST(void)
{
    // ...
}