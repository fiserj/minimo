$input v_color0, v_texcoord0

#include "../../lib/shaders/common.sh"

SAMPLER2D(s_tex_color_rgba, 0);

void main()
{
    gl_FragColor = v_color0 * texture2D(s_tex_color_rgba, FIX_TEXCOORD(v_texcoord0));
}
