#pragma once

namespace mnm
{

// -----------------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------------

static inline u16 mesh_type(u32 flags)
{
    constexpr u16 types[] =
    {
        MESH_STATIC,
        MESH_TRANSIENT,
        MESH_DYNAMIC,
        MESH_INVALID,
    };

    return types[((flags & MESH_TYPE_MASK) >> MESH_TYPE_SHIFT)];
}


// -----------------------------------------------------------------------------
// MESH
// -----------------------------------------------------------------------------

union VertexBufferUnion
{
    u16                        transient_index;// = bgfx::kInvalidHandle;
    bgfx::VertexBufferHandle        static_buffer;
    bgfx::DynamicVertexBufferHandle dynamic_buffer;
};

union IndexBufferUnion
{
    u16                       transient_index;// = bgfx::kInvalidHandle;
    bgfx::IndexBufferHandle        static_buffer;
    bgfx::DynamicIndexBufferHandle dynamic_buffer;
};

struct Mesh
{
    u32          element_count;// = 0;
    u32          extra_data;//    = 0;
    u32          flags;//         = MESH_INVALID;
    VertexBufferUnion positions;
    VertexBufferUnion attribs;
    IndexBufferUnion  indices;
    u8           _pad[2];

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

constexpr Mesh EMPTY_MESH =
{
    0, // u32          element_count;// = 0;
    0, // u32          extra_data;//    = 0;
    MESH_INVALID, // u32          flags;//         = MESH_INVALID;
    BGFX_INVALID_HANDLE, // VertexBufferUnion positions;
    BGFX_INVALID_HANDLE, // VertexBufferUnion attribs;
    BGFX_INVALID_HANDLE, // IndexBufferUnion  indices;
    // u8           _pad[2];
};


// -----------------------------------------------------------------------------
// MESH CACHE
// -----------------------------------------------------------------------------

struct MeshCache
{
public:
    void init()
    {
        m_meshes.fill(EMPTY_MESH);
    }

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

        // for (Mesh& mesh : m_meshes)
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

            m_meshes[m_transient_idxs[i]] = EMPTY_MESH;
        }

        m_transient_idxs   .clear();
        m_transient_buffers.clear();

        m_transient_exhausted = false;
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
        (void)memcpy(m_transient_buffers.back().data, data.data, data.size);

        return true;
    }

    bool add_transient_mesh(Mesh& mesh, const MeshRecorder& recorder, const VertexLayoutCache& layouts)
    {
        ASSERT(!recorder.position_buffer.is_empty());

        if (!m_transient_exhausted)
        {
            if (!add_transient_buffer(recorder.position_buffer, layouts[VERTEX_POSITION], mesh.positions.transient_index) ||
                !add_transient_buffer(recorder.attrib_buffer  , layouts[mesh.flags     ], mesh.attribs  .transient_index)
            )
            {
                m_transient_exhausted = true;
                mesh = EMPTY_MESH;
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
        streams[0] = { recorder.position_buffer.data, layouts[0]->getStride(), layouts[0]->getStride() };

        const bool has_attribs = (mesh_attribs(mesh.flags) & VERTEX_ATTRIB_MASK);

        if (has_attribs)
        {
            layouts[1] = &layout_cache[mesh.flags];
            streams[1] = { recorder.attrib_buffer.data, layouts[1]->getStride(), layouts[1]->getStride() };
        }

        DynamicArray<u32> remap_table; // TODO : This should use some scratch / frame memory.
        remap_table.resize(mesh.element_count);

        u32 indexed_vertex_count = 0;

        if (has_attribs)
        {
            indexed_vertex_count = u32(meshopt_generateVertexRemapMulti(
                remap_table.data, nullptr, mesh.element_count, mesh.element_count, streams, BX_COUNTOF(streams)
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
            indexed_vertex_count = u32(meshopt_generateVertexRemap(
                remap_table.data, nullptr, mesh.element_count, streams[0].data, mesh.element_count, streams[0].size
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
                static_cast<f32*>(vertex_positions),
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
                static_cast<f32*>(vertex_positions),
                mesh.indices.dynamic_buffer
            );
        }
    }

    template <typename BufferT>
    inline static void update_persistent_vertex_buffer
    (
        const meshopt_Stream&       stream,
        const bgfx::VertexLayout&   layout,
        u32                    vertex_count,
        u32                    indexed_vertex_count,
        const DynamicArray<u32>& remap_table,
        BufferT&                    dst_buffer_handle,
        void**                      dst_remapped_memory = nullptr
    )
    {
        static_assert(
            std::is_same<BufferT, bgfx::       VertexBufferHandle>::value ||
            std::is_same<BufferT, bgfx::DynamicVertexBufferHandle>::value,
            "Unsupported vertex buffer type for update."
        );

        const bgfx::Memory* memory = bgfx::alloc(u32(indexed_vertex_count * stream.size));
        ASSERT(memory && memory->data);

        meshopt_remapVertexBuffer(memory->data, stream.data, vertex_count, stream.size, remap_table.data);

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
        u32                    vertex_count,
        u32                    indexed_vertex_count,
        const DynamicArray<u32>& remap_table,
        bool                        optimize,
        const f32*                vertex_positions,
        T*                          dst_indices
    )
    {
        meshopt_remapIndexBuffer<T>(dst_indices, nullptr, vertex_count, remap_table.data);

        if (optimize && vertex_positions)
        {
            meshopt_optimizeVertexCache<T>(dst_indices, dst_indices, vertex_count, indexed_vertex_count);

            meshopt_optimizeOverdraw(dst_indices, dst_indices, vertex_count, vertex_positions, indexed_vertex_count, 3 * sizeof(f32), 1.05f);

            // meshopt_optimizeVertexFetch(vertices, dst_indices, vertex_count, vertex_positions, indexed_vertex_count, 3 * sizeof(f32));
        }
    }

    template <typename BufferT>
    inline static void update_persistent_index_buffer
    (
        u32                    vertex_count,
        u32                    indexed_vertex_count,
        const DynamicArray<u32>& remap_table,
        bool                        optimize,
        const f32*                vertex_positions,
        BufferT&                    dst_buffer_handle
    )
    {
        static_assert(
            std::is_same<BufferT, bgfx::       IndexBufferHandle>::value ||
            std::is_same<BufferT, bgfx::DynamicIndexBufferHandle>::value,
            "Unsupported index buffer type for update."
        );

        u16 buffer_flags = BGFX_BUFFER_NONE;
        u32 type_size    = sizeof(u16);

        if (indexed_vertex_count > UINT16_MAX)
        {
            buffer_flags = BGFX_BUFFER_INDEX32;
            type_size    = sizeof(u32);
        }

        const bgfx::Memory* memory = bgfx::alloc(vertex_count * type_size);
        ASSERT(memory && memory->data);

        type_size == sizeof(u16)
            ? remap_index_buffer(vertex_count, indexed_vertex_count, remap_table, optimize, vertex_positions, reinterpret_cast<u16*>(memory->data))
            : remap_index_buffer(vertex_count, indexed_vertex_count, remap_table, optimize, vertex_positions, reinterpret_cast<u32*>(memory->data));

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
    StaticArray<Mesh, MAX_MESHES>             m_meshes;
    DynamicArray<u16>                    m_transient_idxs;
    DynamicArray<bgfx::TransientVertexBuffer> m_transient_buffers;
    bool                                m_transient_exhausted = false;
};

} // namespace mnm
