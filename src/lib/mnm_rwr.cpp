#include <inttypes.h>     // PRI*
#include <stdint.h>       // *int*_t, UINT*_MAX, uintptr_t

#include <type_traits>    // alignment_of, is_standard_layout, is_trivial, is_trivially_copyable

#define BX_CONFIG_DEBUG
#include <bx/allocator.h> // AllocatorI, BX_ALIGNED_*
#include <bx/bx.h>        // BX_ASSERT, BX_WARN, memCmp, memCopy, min/max


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
// ASSERTION MACROS
// -----------------------------------------------------------------------------

#define ASSERT BX_ASSERT
#define WARN BX_WARN


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
inline void fill_value(void* dst, const T& value, u32 count)
{
    fill_pattern(dst, &value, sizeof(T), count);
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
        reserve(capacity_hint(array.capacity, array.size + 1));
    }

    bx::memCopy(array.data + array.size, &element, sizeof(T));

    array.size++;
}

template <typename T>
T& pop(DynamicArray<T>& array)
{
    ASSERT(array.size);

    return array.data[array.size--];
}


// -----------------------------------------------------------------------------


} // namespace rwr

} // namespace mnm