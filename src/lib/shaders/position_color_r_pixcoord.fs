$input v_color0, v_texcoord0

#include "common.sh"

SAMPLER2D(s_tex_color_r, 0);

uniform vec4 u_tex_size; // Size in `xy`, inverse size in `zw`.

void main()
{
    float alpha  = texture2D(s_tex_color_r, FIX_TEXCOORD(v_texcoord0) * u_tex_size.zw).r;
    gl_FragColor = vec4(v_color0.rgb, v_color0.a * alpha);
}
