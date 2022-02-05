$input  a_position
$output v_color0, v_texcoord0

#include <bgfx_shader.sh>

#define u_texel_size                       u_atlas_info[0].x // 1 / texture_size
#define u_glyph_cols                       u_atlas_info[0].y
#define u_glyph_texel_size                 u_atlas_info[0].zw
#define u_glyph_texel_to_screen_size_ratio u_atlas_info[1].xy

#define u_rect_color(i)                    u_atlas_info[ 2 + i] // NOTE : `numel(u_atlas_info) - 4 - 32`.
#define u_clip_rect(i)                     u_atlas_info[34 + i] // NOTE : `numel(u_atlas_info) - 4`.

// - Atlas info ( 2 x `vec4`).
// - Colors     (32 x `vec4`).
// - Clip rects ( 4 x `vec4`).
uniform vec4 u_atlas_info[38]; // NOTE : Keep in sync with `Uniforms::COUNT`.

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
    // Decode vertex properties.
    const float vertex_index = mod(a_position.z         ,  4.0);
    const float clip_index   = mod(a_position.z * 0.25  ,  4.0);
    const float color_index  = mod(a_position.z * 0.0625, 32.0);
    const float glyph_index  =     a_position.z * 0.001953125  ; // 1 / 4 / 4 / 32

    // Clip vertex position.
    const vec4 clip          = u_clip_rect(int(clip_index));
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
    v_color0    = u_rect_color(int(color_index));
}
