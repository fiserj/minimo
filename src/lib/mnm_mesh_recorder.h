#pragma once

namespace mnm
{

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
