$input  a_position, a_color0, i_data0, i_data1, i_data2, i_data3
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    mat4 model  = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    gl_Position = mul(u_viewProj, mul(model, vec4(a_position, 1.0)));
    v_color0    = a_color0;
}
