#pragma once

namespace mnm
{

internal void generate_flat_normals
(
    u32                                  vertex_count,
    u32                                  vertex_stride,
    const Vec3*                          vertices,
    VertexAttribState::PackedNormalType* normals
)
{
    ASSERT(vertex_count % 3 == 0);

    for (u32 i = 0; i < vertex_count; i += 3)
    {
        const Vec3 a = vertices[i + 1] - vertices[i];
        const Vec3 b = vertices[i + 2] - vertices[i];
        const Vec3 n = HMM_Normalize(HMM_Cross(a, b));

        const f32 normalized[] =
        {
            n.X * 0.5f + 0.5f,
            n.Y * 0.5f + 0.5f,
            n.Z * 0.5f + 0.5f,
        };

        bx::packRgb8(&normals[i], normalized);

        normals[i + vertex_stride    ] = normals[i];
        normals[i + vertex_stride * 2] = normals[i];
    }
}

struct MeshRecorder
{
    void reset(u32 flags)
    {
        position_buffer.clear();
        attrib_buffer  .clear();

        attrib_funcs     = s_attrib_state_func_table[flags];
        vertex_func      = s_vertex_push_func_table [flags];
        vertex_count     = 0;
        invocation_count = 0;
    }

    inline void clear()
    {
        reset(0);

        attrib_funcs = {};
        vertex_func  = {};
    }

    inline void vertex(const Vec3& position)
    {
        (* vertex_func)(
            position,
            attrib_state,
            attrib_buffer,
            position_buffer,
            vertex_count,
            invocation_count
        );
    }

    inline void color(u32 rgba)
    {
        attrib_funcs.color(attrib_state, rgba);
    }

    inline void normal(f32 nx, f32 ny, f32 nz)
    {
        attrib_funcs.normal(attrib_state, nx, ny, nz);
    }

    inline void texcoord(f32 u, f32 v)
    {
        attrib_funcs.texcoord(attrib_state, u, v);
    }

    void generate_flat_normals(u32 flags)
    {
        const u32 offset = vertex_attrib_offset(flags, VERTEX_NORMAL);
        const u32 stride = vertex_attribs_size (flags);

        ::mnm::generate_flat_normals(
            vertex_count,
            stride / sizeof(VertexAttribState::PackedNormalType),
            reinterpret_cast<Vec3*>(position_buffer.data),
            reinterpret_cast<VertexAttribState::PackedNormalType*>(attrib_buffer.data + offset)
        );
    }

    DynamicArray<u8>         attrib_buffer;
    DynamicArray<u8>         position_buffer;
    VertexAttribState        attrib_state;
    VertexAttribStateFuncSet attrib_funcs;
    VertexStoreFunc          vertex_func;
    u32                      vertex_count;
    u32                      invocation_count;

    static VertexAttribStateFuncTable s_attrib_state_func_table;
    static VertexStoreFuncTable       s_vertex_push_func_table;
};

VertexAttribStateFuncTable MeshRecorder::s_attrib_state_func_table;
VertexStoreFuncTable       MeshRecorder::s_vertex_push_func_table;

} // namespace mnm
