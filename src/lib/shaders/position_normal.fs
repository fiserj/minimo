$input v_normal

#include <bgfx_shader.sh>
#include <shaderlib.sh>

void main()
{
    gl_FragColor = vec4(encodeNormalUint(normalize(v_normal)), 1.0);
}
