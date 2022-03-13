#include <mnm/mnm.h>

#include <inttypes.h>     // PRI*
#include <stdint.h>       // *int*_t, UINT*_MAX, uintptr_t

#include <type_traits>    // alignment_of, is_standard_layout, is_trivial, is_trivially_copyable

#ifdef NDEBUG
#   define BX_CONFIG_DEBUG 0
#else
#   define BX_CONFIG_DEBUG 1
#endif

#include <bx/allocator.h> // AllocatorI, BX_ALIGNED_*
#include <bx/bx.h>        // BX_ASSERT, BX_CONCATENATE, BX_WARN, memCmp, memCopy, min/max


namespace mnm
{

namespace rwr
{

// -----------------------------------------------------------------------------
// FIXED-SIZE TYPES
// -----------------------------------------------------------------------------

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

constexpr u8  U8_MAX  = UINT8_MAX;
constexpr u16 U16_MAX = UINT16_MAX;
constexpr u32 U32_MAX = UINT32_MAX;


// -----------------------------------------------------------------------------
// CONSTANTS (RESOURCE LIMITS, DEFAULT VALUES, FLAG MASKS & SHIFTS)
// -----------------------------------------------------------------------------

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


// -----------------------------------------------------------------------------
// ASSERTION MACROS
// -----------------------------------------------------------------------------

#define ASSERT BX_ASSERT
#define WARN BX_WARN


// -----------------------------------------------------------------------------
// UNIT TESTING
// -----------------------------------------------------------------------------

#ifndef CONFIG_TESTING
#   define CONFIG_TESTING BX_CONFIG_DEBUG
#endif

#if CONFIG_TESTING

#define TEST_CASE(name)                                            \
    static void BX_CONCATENATE(s_test_func_, __LINE__)();          \
    static const bool BX_CONCATENATE(s_test_var_, __LINE__) = []() \
    {                                                              \
        BX_CONCATENATE(s_test_func_, __LINE__)();                  \
        return true;                                               \
    }();                                                           \
    void BX_CONCATENATE(s_test_func_, __LINE__)()

#define TEST_REQUIRE(cond) ASSERT(cond, #cond)

#else

#define TEST_CASE(name) [[maybe_unused]] \
    void BX_CONCATENATE(s_unused_func_, __LINE__)()

#define TEST_REQUIRE(cond) BX_NOOP(cond)

#endif // CONFIG_TESTING


// -----------------------------------------------------------------------------
// UTILITY FUNCTIONS
// -----------------------------------------------------------------------------

template <typename T>
constexpr bool is_pod()
{
    // `std::is_pod` is being deprecated as of C++20.
    return std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}

static bool is_aligned(const void* ptr, size_t alignment)
{
    return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

static constexpr u64 s_zero_memory[8] = {};

static void fill_pattern(void* dst, const void* pattern, u32 size, u32 count)
{
    ASSERT(dst, "Invalid dst pointer.");
    ASSERT(pattern, "Invalid pattern pointer.");
    ASSERT(size, "Zero size.");
    ASSERT(count, "Zero count.");

    if (size <= sizeof(s_zero_memory) &&
        0 == bx::memCmp(pattern, s_zero_memory, size))
    {
        bx::memSet(dst, 0, size * count);
    }
    else
    {
        for (u32 i = 0, n = count * size; i < n; i += size)
        {
            bx::memCopy(static_cast<u8*>(dst) + i, pattern, size);
        }
    }
}

template <typename T>
void fill_value(void* dst, const T& value, u32 count)
{
    fill_pattern(dst, &value, sizeof(T), count);
}


// -----------------------------------------------------------------------------
// DEFERRED EXECUTION
// -----------------------------------------------------------------------------

template <typename Func>
struct Deferred
{
    Func func;

    Deferred(const Deferred&) = delete;

    Deferred& operator=(const Deferred&) = delete;

    Deferred(Func&& func)
        : func(static_cast<Func&&>(func))
    {
    }

    ~Deferred()
    {
        func();
    }
};

template <typename Func>
Deferred<Func> make_deferred(Func&& func)
{
    return Deferred<Func>(static_cast<decltype(func)>(func));
}

#define defer(...) auto BX_CONCATENATE(deferred_ , __LINE__) = \
    make_deferred([&]() mutable { __VA_ARGS__; })

TEST_CASE("Deferred Execution")
{
    int value = 1;

    {
        defer(value++);

        {
            defer(
                for (int i = 0; i < 3; i++)
                {
                    value++;
                }
            );

            TEST_REQUIRE(value == 1);
        }

        TEST_REQUIRE(value == 4);
    }

    TEST_REQUIRE(value == 5);
}


// -----------------------------------------------------------------------------
// ALLOCATORS
// -----------------------------------------------------------------------------

using Allocator  = bx::AllocatorI;

using CrtAllocator = bx::DefaultAllocator;

// ...


// -----------------------------------------------------------------------------
// FIXED ARRAY
// -----------------------------------------------------------------------------

template <typename T, u32 Size>
struct FixedArray
{
    static_assert(
        std::is_trivially_copyable<T>(),
        "`FixedArray` only supports trivially copyable types."
    );

    static_assert(
        Size > 0,
        "`FixedArray` must have positive size."
    );

    static constexpr u32 size = Size;

    T data[Size];

    const T& operator[](u32 i) const
    {
        ASSERT(i < Size, "Index %" PRIu32 "out of range %" PRIu32 ".", i, Size);

        return data[i];
    }

    T& operator[](u32 i)
    {
        ASSERT(i < Size, "Index %" PRIu32 "out of range %" PRIu32 ".", i, Size);

        return data[i];
    }
};


// -----------------------------------------------------------------------------
// DYNAMIC ARRAY
// -----------------------------------------------------------------------------

template <typename T>
struct DynamicArray
{
    static_assert(
        is_pod<T>(),
        "`DynamicArray` only supports POD-like types."
    );

    T*              data;
    u32             size;
    u32             capacity;
    bx::AllocatorI* allocator;

    const T& operator[](u32 i) const
    {
        ASSERT(data, "Invalid data pointer.");
        ASSERT(i < size, "Index %" PRIu32 "out of range %" PRIu32 ".", i, size);

        return data[i];
    }

    T& operator[](u32 i)
    {
        ASSERT(data, "Invalid data pointer.");
        ASSERT(i < size, "Index %" PRIu32 "out of range %" PRIu32 ".", i, size);

        return data[i];
    }
};

template <typename T>
DynamicArray<T> create_dynamic_array(Allocator* allocator)
{
    ASSERT(allocator, "Invalid allocator pointer.");

    DynamicArray<T> array = {};
    array.allocator = allocator;

    return array;
}

template <typename T>
void destroy(DynamicArray<T>& array)
{
    ASSERT(array.allocator, "Invalid allocator pointer.");

    BX_ALIGNED_FREE(array.allocator, array.data, std::alignment_of<T>::value);

    array = {};
}

static u32 capacity_hint(u32 capacity, u32 requested_size)
{
    return bx::max(u32(8), requested_size, capacity + capacity / 2);
}

template <typename T>
void reserve(DynamicArray<T>& array, u32 capacity)
{
    if (capacity > array.capacity)
    {
        T* data = static_cast<T*>(BX_ALIGNED_REALLOC(
            array.allocator,
            array.data,
            capacity * sizeof(T),
            std::alignment_of<T>::value
        ));

        ASSERT(data, "Data reallocation failed.");

        if (data)
        {
            array.data     = data;
            array.capacity = capacity;
        }
    }
}

template <typename T>
void resize(DynamicArray<T>& array, u32 size)
{
    if (size > array.capacity)
    {
        reserve(array, capacity_hint(array.capacity, size));
    }

    array.size = size;
}

template <typename T>
void resize(DynamicArray<T>& array, u32 size, const T& element)
    {
        const u32 old_size = array.size;

        resize(array, size);

        if (array.size > old_size)
        {
            fill_value(array.data + old_size, element, array.size - old_size);
        }
    }

template <typename T>
T& append(DynamicArray<T>& array, const T& element)
{
    ASSERT(&element < array.data || &element >= array.data + array.capacity,
        "Cannot append element stored in the array.");

    if (array.size == array.capacity)
    {
        reserve(array, capacity_hint(array.capacity, array.size + 1));
    }

    bx::memCopy(array.data + array.size, &element, sizeof(T));

    return array.data[array.size++];
}

template <typename T>
T& pop(DynamicArray<T>& array)
{
    ASSERT(array.size, "Cannot pop from an empty array.");

    return array.data[array.size--];
}

TEST_CASE("Dynamic Array")
{
    CrtAllocator allocator;

    auto array = create_dynamic_array<int>(&allocator);
    TEST_REQUIRE(array.allocator == &allocator);

    reserve(array, 3);
    TEST_REQUIRE(array.size == 0);
    TEST_REQUIRE(array.capacity >= 3);

    int val = append(array, 10);
    TEST_REQUIRE(array.size == 1);
    TEST_REQUIRE(array[0] == 10);
    TEST_REQUIRE(val == 10);

    val = append(array, 20);
    TEST_REQUIRE(array.size == 2);
    TEST_REQUIRE(array[1] == 20);
    TEST_REQUIRE(val == 20);

    val = append(array, 30);
    TEST_REQUIRE(array.size == 3);
    TEST_REQUIRE(array[2] == 30);
    TEST_REQUIRE(val == 30);

    val = pop(array);
    TEST_REQUIRE(array.size == 2);
    TEST_REQUIRE(val == 30);

    resize(array, 10, 100);
    TEST_REQUIRE(array.size == 10);
    TEST_REQUIRE(array.capacity >= 10);

    for (u32 i = 2; i < array.size; i++)
    {
        TEST_REQUIRE(array[i] == 100);
    }

    destroy(array);
    TEST_REQUIRE(array.data == nullptr);
    TEST_REQUIRE(array.size == 0);
    TEST_REQUIRE(array.capacity == 0);
    TEST_REQUIRE(array.allocator == nullptr);
}


// -----------------------------------------------------------------------------
// FIXED STACK
// -----------------------------------------------------------------------------

template <typename T, u32 Size>
struct FixedStack
{
    T                   top;
    u32                 size;
    FixedArray<T, Size> data;
};

template <typename T, u32 Size>
void reset(FixedStack<T, Size>& stack, const T& value = T())
{
    stack.top  = value;
    stack.size = 0;
}

template <typename T, u32 Size>
void push(FixedStack<T, Size>& stack)
{
    stack.data[stack.size++] = stack.top;
}

template <typename T, u32 Size>
T& pop(FixedStack<T, Size>& stack)
{
    stack.top = stack.data[--stack.size];

    return stack.top;
}


// -----------------------------------------------------------------------------


} // namespace rwr

} // namespace mnm