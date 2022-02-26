$input  a_position, a_color0, a_normal
$output v_color0, v_normal

#include <bgfx_shader.sh>
#include <shaderlib.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal    = mul(u_modelView, vec4(decodeNormalUint(a_normal), 0.0)).xyz;
    v_color0    = a_color0;
}
