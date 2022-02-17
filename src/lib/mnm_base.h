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

using Allocator  = bx::AllocatorI;
using Mutex      = bx::Mutex;
using MutexScope = bx::MutexScope;

using Mat4 = hmm_mat4;
using Vec2 = hmm_vec2;
using Vec3 = hmm_vec3;
using Vec4 = hmm_vec4;

internal Allocator* default_allocator()
{
    static bx::DefaultAllocator s_allocator;
    return &s_allocator;
}

template <typename T>
constexpr bool is_pod()
{
    // `std::is_pod` is being deprecated as of C++20.
    return std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}

internal inline bool is_aligned(const void* ptr, size_t alignment)
{
    return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

template <u32 Size>
inline void assign(const void* src, void* dst)
{
    struct Block
    {
        u8 bytes[Size];
    };

    ASSERT(is_aligned(src, std::alignment_of<Block>::value));
    ASSERT(is_aligned(dst, std::alignment_of<Block>::value));

    *static_cast<Block*>(dst) = *static_cast<const Block*>(src);
}

} // namespace mnm
