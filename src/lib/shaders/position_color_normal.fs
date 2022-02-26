$input v_color0, v_normal

#include <bgfx_shader.sh>
#include <shaderlib.sh>

void main()
{
    vec3 light_dir = vec3(0.0, 0.0, 1.0); // TODO : Expose via a uniform?
    vec3 normal    = normalize(v_normal);
    vec3 diffuse   = vec3_splat(max(dot(normal, light_dir), 0.0));

    gl_FragColor = vec4(diffuse * v_color0.rgb, 1.0);
}
