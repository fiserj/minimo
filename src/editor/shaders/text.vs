$input  a_position
$output v_color0, v_texcoord0

#include <bgfx_shader.sh>

#define texel_size u_atlas_info.x
#define glyph_cols u_atlas_info.y
#define glyph_size u_atlas_info.zw

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

// TODO : Move to a uniform.
// NOTE : Colors with zero alpha are for text rendering, the remaining ones are
//        for filled rectangle rendering (see `text.fs` for exaplanation).
const vec4 colors[] =
{
    vec4(1.00, 1.00, 1.00, 0.00), // COLOR_EDITOR_TEXT
    vec4(0.67, 0.67, 0.67, 0.00), // COLOR_EDITOR_LINE_NUMBER

    vec4(1.00, 0.00, 0.00, 1.00), // COLOR_RED
    vec4(0.00, 1.00, 0.00, 1.00), // COLOR_GREEN
    vec4(0.00, 0.00, 1.00, 1.00), // COLOR_BLUE
    vec4(0.00, 0.00, 0.00, 1.00)  // COLOR_BLACK,
};

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position.xy, 0.0, 1.0));

    // Decode vertex properties.
    const float vertex_index = mod(a_position.z, 4.0);
    const float clip_index   = mod(a_position.z / 4.0, 4.0);
    const float color_index  = mod(a_position.z / 16.0, 16.0);
    const float glyph_index  = a_position.z / 256.0;

    const float col = mod  (glyph_index , glyph_cols);
    const float row = floor(glyph_index / glyph_cols);

    vec2 uv = (vec2(col, row) + offset[int(vertex_index)]) * glyph_size;
    uv.x -= step(1.5, vertex_index) * texel_size; // Remove X-padding from the right hand side of the quad.

    v_texcoord0 = uv;
    v_color0    = colors[int(color_index)];
}
