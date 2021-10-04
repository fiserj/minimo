$input v_color0, v_texcoord0

#include "common.sh"

SAMPLER2D(s_tex_color_r, 0);

// TODO !!! A uniform with the size of the atlas is needed.

void main()
{
    float alpha  = texture2D(s_tex_color_r, FIX_TEXCOORD(v_texcoord0) * 0.00390625).r; // 1 / 256
    gl_FragColor = vec4(v_color0.rgb, v_color0.a * alpha);
}
