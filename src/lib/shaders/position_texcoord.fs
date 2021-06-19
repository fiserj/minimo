$input v_texcoord0

#include "common.sh"

SAMPLER2D(s_tex_color, 0);

void main()
{
    gl_FragColor = texture2D(s_tex_color, FIX_TEXCOORD(v_texcoord0));
}
