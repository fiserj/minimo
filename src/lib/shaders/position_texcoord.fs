$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex_color, 0);

void main()
{
    gl_FragColor = texture2D(s_tex_color, v_texcoord0);
}
