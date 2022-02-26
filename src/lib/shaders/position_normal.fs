$input v_normal

#include <bgfx_shader.sh>

void main()
{
    // NOTE : Normals are packed as RGB, so no visualization conversion needed.
    gl_FragColor = vec4(v_normal, 1.0);
}
