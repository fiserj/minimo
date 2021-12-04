$input  a_position
$output v_color0, v_texcoord0

#include <bgfx_shader.sh>

#define u_texel_size                       u_atlas_info[0].x // 1 / texture_size
#define u_glyph_cols                       u_atlas_info[0].y
#define u_glyph_texel_size                 u_atlas_info[0].zw
#define u_glyph_texel_to_screen_size_ratio u_atlas_info[1].xy

#define u_clip_rect(i)                     u_atlas_info[18 + i]

uniform vec4 u_atlas_info[22]; // NOTE : Size has to be kept in sync with `Uniforms::COUNT`.

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
    // Decode vertex properties.
    const float vertex_index = mod(a_position.z, 4.0);
    const float clip_index   = mod(a_position.z * 0.25, 4.0);
    const float color_index  = mod(a_position.z * 0.0625, 16.0);
    const float glyph_index  = a_position.z * 0.00390625;

    // Clip vertex position.
    const vec4 clip          = u_clip_rect(int(clip_index));//vec4(0.0, 0.0, 600.0, 500.0);
    const vec2 position      = clamp(a_position.xy, clip.xy, clip.zw);
    const vec2 position_diff = position - a_position.xy;
    gl_Position              = mul(u_modelViewProj, vec4(position, 0.0, 1.0));

    // Determine glyph position in atlas.
    const float col = mod  (glyph_index , u_glyph_cols);
    const float row = floor(glyph_index / u_glyph_cols);

    vec2 uv = (vec2(col, row) + offset[int(vertex_index)]) * u_glyph_texel_size;

    // Remove X-padding from the right hand side of the quad.
    uv.x -= step(1.5, vertex_index) * u_texel_size;

    // Clip coordinates.
    uv += position_diff * u_glyph_texel_to_screen_size_ratio;

    v_texcoord0 = uv;
    v_color0    = colors[int(color_index)];
}
