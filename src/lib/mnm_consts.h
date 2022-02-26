#pragma once

namespace mnm
{

constexpr i32 DEFAULT_WINDOW_HEIGHT  = 600;
constexpr i32 DEFAULT_WINDOW_WIDTH   = 800;
constexpr i32 MIN_WINDOW_SIZE        = 240;

constexpr u32 ATLAS_FREE             = 0x08000;
constexpr u32 ATLAS_MONOSPACED       = 0x00002;
constexpr u32 MESH_INVALID           = 0x00006;
constexpr u32 VERTEX_POSITION        = 0x00000;
constexpr u32 VERTEX_TEXCOORD_F32    = VERTEX_TEXCOORD | TEXCOORD_F32;

// These have to be cross-checked against regular mesh flags (see later).
constexpr u32 INSTANCING_SUPPORTED   = 0x100000;
constexpr u32 SAMPLER_COLOR_R        = 0x200000;
constexpr u32 TEXT_MESH              = 0x400000;
constexpr u32 VERTEX_PIXCOORD        = 0x800000;

constexpr u32 MAX_FONTS              = 128;
constexpr u32 MAX_FRAMEBUFFERS       = 128;
constexpr u32 MAX_INSTANCE_BUFFERS   = 16;
constexpr u32 MAX_MESHES             = 4096;
constexpr u32 MAX_PASSES             = 64;
constexpr u32 MAX_PROGRAMS           = 128;
constexpr u32 MAX_TASKS              = 64;
constexpr u32 MAX_TEXTURES           = 1024;
constexpr u32 MAX_TEXTURE_ATLASES    = 32;
constexpr u32 MAX_UNIFORMS           = 256;

constexpr u16 MESH_TYPE_MASK         = MESH_STATIC | MESH_TRANSIENT | MESH_DYNAMIC | MESH_INVALID;
constexpr u16 MESH_TYPE_SHIFT        = 1;

constexpr u16 PRIMITIVE_TYPE_MASK    = PRIMITIVE_TRIANGLES | PRIMITIVE_QUADS | PRIMITIVE_TRIANGLE_STRIP |
                                       PRIMITIVE_LINES | PRIMITIVE_LINE_STRIP | PRIMITIVE_POINTS;
constexpr u16 PRIMITIVE_TYPE_SHIFT   = 4;

constexpr u16 TEXT_H_ALIGN_MASK      = TEXT_H_ALIGN_LEFT | TEXT_H_ALIGN_CENTER | TEXT_H_ALIGN_RIGHT;
constexpr u16 TEXT_H_ALIGN_SHIFT     = 4;
constexpr u16 TEXT_TYPE_MASK         = TEXT_STATIC | TEXT_TRANSIENT | TEXT_DYNAMIC;
constexpr u16 TEXT_V_ALIGN_MASK      = TEXT_V_ALIGN_BASELINE | TEXT_V_ALIGN_MIDDLE | TEXT_V_ALIGN_CAP_HEIGHT;
constexpr u16 TEXT_V_ALIGN_SHIFT     = 7;
constexpr u16 TEXT_Y_AXIS_MASK       = TEXT_Y_AXIS_UP | TEXT_Y_AXIS_DOWN;
constexpr u16 TEXT_Y_AXIS_SHIFT      = 10;

constexpr u16 TEXTURE_BORDER_MASK    = TEXTURE_MIRROR | TEXTURE_CLAMP;
constexpr u16 TEXTURE_BORDER_SHIFT   = 1;
constexpr u16 TEXTURE_FORMAT_MASK    = TEXTURE_R8 | TEXTURE_D24S8 | TEXTURE_D32F;
constexpr u16 TEXTURE_FORMAT_SHIFT   = 3;
constexpr u16 TEXTURE_SAMPLING_MASK  = TEXTURE_NEAREST;
constexpr u16 TEXTURE_SAMPLING_SHIFT = 0;
constexpr u16 TEXTURE_TARGET_MASK    = TEXTURE_TARGET;
constexpr u16 TEXTURE_TARGET_SHIFT   = 6;

constexpr u16 VERTEX_ATTRIB_MASK     = VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD;
constexpr u16 VERTEX_ATTRIB_SHIFT    = 7; // VERTEX_COLOR => 1 (so that VERTEX_POSITION is zero)

constexpr u32 USER_MESH_FLAGS        = MESH_TYPE_MASK | PRIMITIVE_TYPE_MASK | VERTEX_ATTRIB_MASK | TEXCOORD_F32 |
                                       OPTIMIZE_GEOMETRY | NO_VERTEX_TRANSFORM | KEEP_CPU_GEOMETRY |
                                       GENEREATE_SMOOTH_NORMALS | GENEREATE_FLAT_NORMALS;
constexpr u32 INTERNAL_MESH_FLAGS    = INSTANCING_SUPPORTED | SAMPLER_COLOR_R | TEXT_MESH | VERTEX_PIXCOORD;

static_assert(
    0 == (INTERNAL_MESH_FLAGS & USER_MESH_FLAGS),
    "Internal mesh flags interfere with the user-exposed ones."
);

static_assert(
    bx::isPowerOf2(PRIMITIVE_QUADS),
    "`PRIMITIVE_QUADS` must be power of two."
);

} // namespace mnm
