$input v_color0, v_texcoord0

#include "../../lib/shaders/common.sh"

SAMPLER2D(s_tex_color_r, 0);

void main()
{
    float alpha  = texture2D(s_tex_color_r, FIX_TEXCOORD(v_texcoord0)).r;

    // NOTE : The `max` call is a hack to avoid the need of either encoding a
    //        background color (or at least information about being opaque), or
    //        creating a fully opaque glyph withn the glyph texture cache. The
    //        downside is that the same color for text and quads has to be
    //        provided twice.
    gl_FragColor = vec4(v_color0.rgb, max(v_color0.a, alpha));
}
