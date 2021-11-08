$input  a_position, a_color0
$output v_color0, v_texcoord0

#include <bgfx_shader.sh>

#define tex_inv_size u_atlas_info.x
#define glyph_cols   u_atlas_info.y
#define glyph_size   u_atlas_info.zw

uniform vec4 u_atlas_info;

// 0 -- 3
// | \  |
// |  \ |
// 1 -- 2
const vec2 offset[] =
{
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
};

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position.xy, 0.0, 1.0));
    v_color0    = a_color0;

    // TODO : Reuse `floor` results in `mod` computations.
    const float vertex_index = mod  (a_position.z , 4.0);
    const float glyph_index  = floor(a_position.z / 4.0);

    const float col = mod  (glyph_index , glyph_cols);
    const float row = floor(glyph_index / glyph_cols);

    vec2 uv = (vec2(col, row) + offset[int(vertex_index)]) * glyph_size;
    uv.x -= step(1.5, vertex_index); // Padding of 1 pixel only in X axis.

    v_texcoord0 = uv * tex_inv_size;
}
