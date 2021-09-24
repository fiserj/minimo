$input v_color0, v_texcoord0

#include "common.sh"

SAMPLER2D(s_tex_color_r, 0);

void main()
{
    float alpha  = texture2D(s_tex_color_r, FIX_TEXCOORD(v_texcoord0)).r;
    gl_FragColor = vec4(v_color0.rgb, v_color0.a * alpha);
}
