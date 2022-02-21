#pragma once

namespace mnm
{

internal inline u16 mesh_type(u32 flags)
{
    constexpr u16 types[] =
    {
        MESH_STATIC,
        MESH_TRANSIENT,
        MESH_DYNAMIC,
        MESH_INVALID,
    };

    return types[(flags & MESH_TYPE_MASK) >> MESH_TYPE_SHIFT];
}

union VertexBufferUnion
{
    u16                             transient_index;
    bgfx::VertexBufferHandle        static_buffer;
    bgfx::DynamicVertexBufferHandle dynamic_buffer;
};

union IndexBufferUnion
{
    u16                            transient_index;
    bgfx::IndexBufferHandle        static_buffer;
    bgfx::DynamicIndexBufferHandle dynamic_buffer;
};

struct Mesh
{
    u32               element_count = 0;
    u32               extra_data    = 0;
    u32               flags         = MESH_INVALID;
    VertexBufferUnion positions     = BGFX_INVALID_HANDLE;
    VertexBufferUnion attribs       = BGFX_INVALID_HANDLE;
    IndexBufferUnion  indices       = BGFX_INVALID_HANDLE;
    u8                _pad[2]       = {};

    inline u16 type() const
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

inline internal VertexBufferUnion create_persistent_vertex_buffer
(
    u16                       type,
    const meshopt_Stream&     stream,
    const bgfx::VertexLayout& layout,
    u32                       vertex_count,
    u32                       remapped_vertex_count,
    const u32*                remap_table,
    void**                    remapped_memory = nullptr
)
{
    ASSERT(type == MESH_STATIC || type == MESH_DYNAMIC);
    ASSERT(remap_table);

    // TODO : This should use some scratch / frame memory.
    const bgfx::Memory* memory = bgfx::alloc(u32(remapped_vertex_count * stream.size));
    ASSERT(memory && memory->data);

    meshopt_remapVertexBuffer(memory->data, stream.data, vertex_count, stream.size, remap_table);

    if (remapped_memory)
    {
        *remapped_memory = memory->data;
    }

    u16 handle = bgfx::kInvalidHandle;

    switch (type)
    {
    case MESH_STATIC:
        handle = bgfx::createVertexBuffer(memory, layout).idx;
        break;
    case MESH_DYNAMIC:
        handle = bgfx::createDynamicVertexBuffer(memory, layout).idx;
        break;
    }

    ASSERT(handle != bgfx::kInvalidHandle);

    return { handle };
}

inline internal IndexBufferUnion create_persistent_index_buffer
(
    u16        type,
    u32        vertex_count,
    u32        indexed_vertex_count,
    const f32* vertex_positions,
    const u32* remap_table,
    bool       optimize
)
{
    ASSERT(type == MESH_STATIC || type == MESH_DYNAMIC);
    ASSERT(remap_table);

    u16 buffer_flags = BGFX_BUFFER_NONE;
    u32 type_size    = sizeof(u16);

    if (indexed_vertex_count > UINT16_MAX)
    {
        buffer_flags = BGFX_BUFFER_INDEX32;
        type_size    = sizeof(u32);
    }

    // meshoptimizer works only with `u32`, so we allocate the memory for it
    // anyway, to avoid doing an additional copy.
    // TODO : This should use some scratch / frame memory.
    const bgfx::Memory* memory = bgfx::alloc(vertex_count * sizeof(u32));
    ASSERT(memory && memory->data);

    u32* indices = reinterpret_cast<u32*>(memory->data);

    meshopt_remapIndexBuffer(indices, nullptr, vertex_count, remap_table);

    if (optimize && vertex_positions)
    {
        meshopt_optimizeVertexCache(indices, indices, vertex_count,
            indexed_vertex_count);

        meshopt_optimizeOverdraw(indices, indices, vertex_count,
            vertex_positions, indexed_vertex_count, 3 * sizeof(f32), 1.05f);

        // TODO : Consider also doing `meshopt_optimizeVertexFetch`?
    }

    if (type_size == sizeof(u16))
    {
        const u32* src = reinterpret_cast<u32*>(memory->data);
        u16*       dst = reinterpret_cast<u16*>(memory->data);

        for (u32 i = 0; i < vertex_count; i++)
        {
            dst[i] = src[i];
        }

        const_cast<bgfx::Memory*>(memory)->size /= 2;
    }

    u16 handle = bgfx::kInvalidHandle;

    switch (type)
    {
    case MESH_STATIC:
        handle = bgfx::createIndexBuffer(memory, buffer_flags).idx;
        break;
    case MESH_DYNAMIC:
        handle = bgfx::createDynamicIndexBuffer(memory, buffer_flags).idx;
        break;
    }

    ASSERT(handle != bgfx::kInvalidHandle);

    return { handle };
}

class MeshCache
{
    Mutex                                     m_mutex;
    StaticArray<Mesh, MAX_MESHES>             m_meshes;
    DynamicArray<u16>                         m_transient_idxs;
    DynamicArray<bgfx::TransientVertexBuffer> m_transient_buffers;
    bool                                      m_transient_memory_exhausted = false;

public:
    bool add_mesh(const MeshRecorder& recorder, const VertexLayoutCache& layouts)
    {
        ASSERT(recorder.id < m_meshes.size());

        MutexScope lock(m_mutex);

        Mesh& mesh = m_meshes[recorder.id];

        const u16 new_type = mesh_type(recorder.flags);

        if (new_type == MESH_INVALID)
        {
            ASSERT(false && "Invalid registered mesh type.");
            return false;
        }

        mesh.destroy();

        mesh.element_count = recorder.vertex_count;
        mesh.extra_data    = recorder.extra_data;
        mesh.flags         = recorder.flags;

        switch (new_type)
        {
        case MESH_STATIC:
        case MESH_DYNAMIC:
            add_persistent_mesh(mesh, recorder, layouts);
            break;

        case MESH_TRANSIENT:
            if (add_transient_mesh(mesh, recorder, layouts))
            {
                m_transient_idxs.push(recorder.id);
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

        for (u32 i = 0; i < m_meshes.size(); i++)
        {
            m_meshes[i].destroy();
        }
    }

    void clear_transient_meshes()
    {
        MutexScope lock(m_mutex);

        for (u32 i = 0; i < m_transient_idxs.size; i++)
        {
            ASSERT(m_meshes[m_transient_idxs[i]].type() == MESH_TRANSIENT);

            m_meshes[m_transient_idxs[i]] = {};
        }

        m_transient_idxs   .clear();
        m_transient_buffers.clear();

        m_transient_memory_exhausted = false;
    }

    inline Mesh& operator[](u16 id) { return m_meshes[id]; }

    inline const Mesh& operator[](u16 id) const { return m_meshes[id]; }

    inline const DynamicArray<bgfx::TransientVertexBuffer>& transient_buffers() const
    {
        return m_transient_buffers;
    }

private:
    bool add_transient_buffer(const DynamicArray<u8>& data, const bgfx::VertexLayout& layout, u16& dst_index)
    {
        ASSERT(layout.getStride() > 0);

        if (data.is_empty())
        {
            return true;
        }

        if (data.size % layout.getStride() != 0)
        {
            ASSERT(false && "Layout does not match data size.");
            return false;
        }

        const u32 count = data.size / layout.getStride();

        if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
        {
            // No assert here as it can happen and we'll just skip that geometry.
            return false;
        }

        ASSERT(m_transient_buffers.size < UINT16_MAX);

        dst_index = u16(m_transient_buffers.size);
        m_transient_buffers.resize(m_transient_buffers.size + 1);

        bgfx::allocTransientVertexBuffer(&m_transient_buffers.back(), count, layout);
        bx::memCopy(m_transient_buffers.back().data, data.data, data.size);

        return true;
    }

    bool add_transient_mesh(Mesh& mesh, const MeshRecorder& recorder, const VertexLayoutCache& layouts)
    {
        ASSERT(!recorder.position_buffer.is_empty());

        if (!m_transient_memory_exhausted)
        {
            if (!add_transient_buffer(recorder.position_buffer, layouts[VERTEX_POSITION], mesh.positions.transient_index) ||
                !add_transient_buffer(recorder.attrib_buffer  , layouts[mesh.flags     ], mesh.attribs  .transient_index)
            )
            {
                m_transient_memory_exhausted = true;
                mesh = {};
            }
        }

        return !m_transient_memory_exhausted;
    }

    void add_persistent_mesh(Mesh& mesh, const MeshRecorder& recorder, const VertexLayoutCache& layout_cache)
    {
        ASSERT(mesh.type() == MESH_STATIC || mesh.type() == MESH_DYNAMIC);

        meshopt_Stream            streams[2];
        const bgfx::VertexLayout* layouts[2];

        // TODO : Eventually add support for 2D position.
        layouts[0] = &layout_cache[VERTEX_POSITION];
        streams[0] = { recorder.position_buffer.data, layouts[0]->getStride(),
            layouts[0]->getStride() };

        const bool has_attribs = (mesh_attribs(mesh.flags) & VERTEX_ATTRIB_MASK);

        if (has_attribs)
        {
            layouts[1] = &layout_cache[mesh.flags];
            streams[1] = { recorder.attrib_buffer.data, layouts[1]->getStride(),
                layouts[1]->getStride() };
        }

        DynamicArray<u32> remap_table; // TODO : This should use some scratch / frame memory.
        remap_table.resize(mesh.element_count);

        u32 indexed_vertex_count = 0;

        if (has_attribs)
        {
            indexed_vertex_count = u32(meshopt_generateVertexRemapMulti(
                remap_table.data, nullptr, mesh.element_count,
                mesh.element_count, streams, BX_COUNTOF(streams)
            ));

            mesh.attribs = create_persistent_vertex_buffer(
                mesh.type(),
                streams[1],
                *layouts[1],
                mesh.element_count,
                indexed_vertex_count,
                remap_table.data
            );
        }
        else
        {
            indexed_vertex_count = u32(meshopt_generateVertexRemap(
                remap_table.data, nullptr, mesh.element_count, streams[0].data,
                mesh.element_count, streams[0].size
            ));
        }

        void* vertex_positions = nullptr;

        mesh.positions = create_persistent_vertex_buffer(
            mesh.type(), streams[0], *layouts[0], mesh.element_count,
            indexed_vertex_count, remap_table.data, &vertex_positions
        );

        const bool optimize_geometry =
            (mesh.flags & OPTIMIZE_GEOMETRY) &&
            ((mesh.flags & PRIMITIVE_TYPE_MASK) <= PRIMITIVE_QUADS);

        mesh.indices = create_persistent_index_buffer(
            mesh.type(), mesh.element_count, indexed_vertex_count,
            static_cast<f32*>(vertex_positions), remap_table.data,
            optimize_geometry
        );
    }
};

} // namespace mnm
