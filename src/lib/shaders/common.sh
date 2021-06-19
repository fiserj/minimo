#ifndef COMMON_SH
#define COMMON_SH

#include <bgfx_shader.sh>

#if BGFX_SHADER_LANGUAGE_GLSL
    #define FIX_TEXCOORD(texcoord) vec2(texcoord.x, 1.0 - texcoord.y)
#else
    #define FIX_TEXCOORD(texcoord) texcoord
#endif

#endif // COMMON_SH
