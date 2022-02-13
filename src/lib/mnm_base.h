#pragma once

namespace mnm
{

#define ASSERT(cond) assert(cond)

#define internal static

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

constexpr u16 U16_MAX = UINT16_MAX;
constexpr u32 U32_MAX = UINT32_MAX;

using Allocator = bx::AllocatorI;

using Mat4 = hmm_mat4;
using Vec2 = hmm_vec2;
using Vec3 = hmm_vec3;
using Vec4 = hmm_vec4;

} // namespace mnm
