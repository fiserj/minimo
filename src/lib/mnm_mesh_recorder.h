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

        // TODO : Accessing `normals` is wrong when attributes contains anything else.

        bx::packRgb8(&normals[i], normalized);

        normals[i + vertex_stride    ] = normals[i];
        normals[i + vertex_stride * 2] = normals[i];
    }
}

internal inline float HMM_AngleVec3(Vec3 Left, Vec3 Right)
{
    return acosf(HMM_Clamp(-1.0f, HMM_DotVec3(Left, Right), 1.0f));
}

internal inline bool HMM_EpsilonEqualVec3(Vec3 Left, Vec3 Right, float eps = 1e-4f)
{
    const Vec3 diff = Left - Right;

    return
        (HMM_ABS(diff.X) < eps) &
        (HMM_ABS(diff.Y) < eps) &
        (HMM_ABS(diff.Z) < eps);
}

internal void generate_smooth_normals
(
    u32                                  vertex_count,
    u32                                  vertex_stride,
    const Vec3*                          vertices,
    VertexAttribState::PackedNormalType* normals
)
{
    ASSERT(vertex_count % 3 == 0);

    // TODO : Use temp / stack allocator.
    DynamicArray<u32> unique;
    unique.resize(vertex_count, 0);

    u32 unique_vertex_count = 0;

    for (u32 i = 0; i < vertex_count; i++)
    for (u32 j = 0; j <= i; j++)
    {
        if (HMM_EpsilonEqualVec3(vertices[i], vertices[j]))
        {
            if (i == j)
            {
                unique[i] = unique_vertex_count;
                unique_vertex_count += (i == j);
            }
            else
            {
                unique[i] = unique[j];
            }

            break;
        }
    }

#ifndef NDEBUG
    for (u32 i = 0; i < vertex_count; i++)
    {
        ASSERT(unique[i] < unique_vertex_count);
    }
#endif // NDEBUG

    union Normal
    {
        Vec3                                full;
        VertexAttribState::PackedNormalType packed;
    };

    // TODO : Use temp / stack allocator.
    DynamicArray<Normal> smooth;
    smooth.resize(unique_vertex_count, Normal{HMM_Vec3(0.0f, 0.0f, 0.0f)});

    // https://stackoverflow.com/a/45496726
    for (u32 i = 0; i < vertex_count; i += 3)
    {
        const Vec3 p0 = vertices[i    ];
        const Vec3 p1 = vertices[i + 1];
        const Vec3 p2 = vertices[i + 2];

        const float a0 = HMM_AngleVec3(HMM_NormalizeVec3(p1 - p0), HMM_NormalizeVec3(p2 - p0));
        const float a1 = HMM_AngleVec3(HMM_NormalizeVec3(p2 - p1), HMM_NormalizeVec3(p0 - p1));
        const float a2 = HMM_AngleVec3(HMM_NormalizeVec3(p0 - p2), HMM_NormalizeVec3(p1 - p2));

        const Vec3 n = HMM_Cross(p1 - p0, p2 - p0);

        smooth[unique[i    ]].full += (n * a0);
        smooth[unique[i + 1]].full += (n * a1);
        smooth[unique[i + 2]].full += (n * a2);
    }

    for (u32 i = 0; i < smooth.size; i++)
    {
        if (!HMM_EqualsVec3(smooth[i].full, HMM_Vec3(0.0f, 0.0f, 0.0f)))
        {
            const Vec3 n = HMM_NormalizeVec3(smooth[i].full);

            const f32 normalized[] =
            {
                n.X * 0.5f + 0.5f,
                n.Y * 0.5f + 0.5f,
                n.Z * 0.5f + 0.5f,
            };

            bx::packRgb8(&smooth[i].packed, normalized);
        }
    }

    for (u32 i = 0, j = 0; i < vertex_count; i++, j += vertex_stride)
    {
        normals[j] = smooth[unique[i]].packed;
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

    void generate_smooth_normals(u32 flags)
    {
        const u32 offset = vertex_attrib_offset(flags, VERTEX_NORMAL);
        const u32 stride = vertex_attribs_size (flags);

        ::mnm::generate_smooth_normals(
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
