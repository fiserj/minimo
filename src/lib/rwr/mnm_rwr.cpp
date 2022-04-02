#include <mnm/mnm.h>

#include <inttypes.h>             // PRI*
#include <stddef.h>               // size_t
#include <stdint.h>               // *int*_t, UINT*_MAX, uintptr_t

#include <type_traits>            // alignment_of, is_standard_layout, is_trivial, is_trivially_copyable, is_unsigned

#include <bx/allocator.h>         // AllocatorI, BX_ALIGNED_*
#include <bx/bx.h>                // BX_ASSERT, BX_CONCATENATE, BX_WARN, memCmp, memCopy, min/max
#include <bx/cpu.h>               // atomicFetchAndAdd, atomicCompareAndSwap
#include <bx/endian.h>            // endianSwap
#include <bx/mutex.h>             // Mutex, MutexScope
#include <bx/pixelformat.h>       // packRg16S, packRgb8

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4127);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);

#include <bgfx/bgfx.h>            // bgfx::*
#include <bgfx/embedded_shader.h> // BGFX_EMBEDDED_SHADER*

BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>            // glfw*

BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wmissing-field-initializers");
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wnested-anon-types");
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4505);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);

#define HANDMADE_MATH_IMPLEMENTATION
#define HMM_STATIC

#include <HandmadeMath.h>          // HMM_*, hmm_*

BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4365);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);

#include <meshoptimizer.h>         // meshopt_*

BX_PRAGMA_DIAGNOSTIC_POP();


namespace mnm
{

namespace rwr
{

namespace
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
constexpr u32 VERTEX_POSITION        = 0x00040;
constexpr u32 VERTEX_TEXCOORD_F32    = VERTEX_TEXCOORD | TEXCOORD_F32;

// These have to be cross-checked against regular mesh flags (see later).
constexpr u32 INSTANCING_SUPPORTED   = 0x100000;
constexpr u32 SAMPLER_COLOR_R        = 0x200000;
constexpr u32 TEXT_MESH              = 0x400000;
constexpr u32 VERTEX_PIXCOORD        = 0x800000;

constexpr u32 MAX_FONTS              = 128;
constexpr u32 MAX_FRAMEBUFFERS       = 128;
constexpr u32 MAX_INSTANCE_BUFFERS   = 32;
constexpr u32 MAX_MESHES             = 4096;
constexpr u32 MAX_PASSES             = 64;
constexpr u32 MAX_PROGRAMS           = 128;
constexpr u32 MAX_TASKS              = 64;
constexpr u32 MAX_TEXTURES           = 1024;
constexpr u32 MAX_TEXTURE_ATLASES    = 32;
constexpr u32 MAX_TRANSIENT_BUFFERS  = 64;
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
constexpr u16 VERTEX_ATTRIB_SHIFT    = 7;

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
    void BX_CONCATENATE(s_test_func_, __LINE__)();          \
    const bool BX_CONCATENATE(s_test_var_, __LINE__) = []() \
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
// CONCURRENCY-RELATED TYPES
// -----------------------------------------------------------------------------

using Mutex      = bx::Mutex;

using MutexScope = bx::MutexScope;


// -----------------------------------------------------------------------------
// UTILITY FUNCTIONS
// -----------------------------------------------------------------------------

constexpr u32 operator""_kB(unsigned long long value)
{
    return value << u32(10);
}

constexpr u32 operator""_MB(unsigned long long value)
{
    return value << u32(20);
}

template <typename T>
constexpr bool is_pod()
{
    // `std::is_pod` is being deprecated as of C++20.
    return std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}

bool is_aligned(const void* ptr, size_t alignment)
{
    return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

constexpr u64 s_zero_memory[8] = {};

void fill_pattern(void* dst, const void* pattern, u32 size, u32 count)
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

template <typename HandleT>
void destroy_if_valid(HandleT& handle)
{
    if (bgfx::isValid(handle))
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }
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
// ALGEBRAIC TYPES
// -----------------------------------------------------------------------------

using Mat4 = hmm_mat4;

using Vec2 = hmm_vec2;

using Vec3 = hmm_vec3;

using Vec4 = hmm_vec4;

union Vec2i
{
    i32 X;
    i32 Y;

    i32& operator[](u32 i)
    {
        ASSERT(i < 2, "Invalid Vec2i index %" PRIu32 ".", i);

        return *(&X + i);
    }
};


// -----------------------------------------------------------------------------
// DEFAULT ALLOCATORS
// -----------------------------------------------------------------------------

using Allocator  = bx::AllocatorI;

using CrtAllocator = bx::DefaultAllocator;


// -----------------------------------------------------------------------------
// STACK ALLOCATOR
// -----------------------------------------------------------------------------

struct StackAllocator : Allocator
{
    enum : u32
    {
        VALID_BIT = 0x80000000,
        SIZE_MASK = 0x7fffffff,
    };

    struct Header
    {
        u32 prev;
        u32 flags;
    };

    struct Block
    {
        Header* header;
        u8*     data;

        u32 size() const
        {
            return header->flags & SIZE_MASK;
        }

        bool is_valid() const
        {
            return header->flags & VALID_BIT;
        }

        void invalidate()
        {
            header->flags &= ~VALID_BIT;
        }

        void reset(u32 prev_, u32 size_)
        {
            header->prev  = prev_;
            header->flags = u32(size_) | VALID_BIT;
        }
    };

    u8* buffer; // First 8 bytes reserved for a sentinel block.
    u32 size;   // Total buffer size in bytes.
    u32 top;    // Offset to first free byte in buffer.
    u32 last;   // Offset of last block header.

    bool owns(const void* ptr) const
    {
        // NOTE : > (not >=) because the first four bytes are reserved for head.
        // TODO : Should really just check against the allocated portion.
        return ptr > buffer && ptr < buffer + size;
    }

    virtual void* realloc(void* ptr, size_t size_, size_t align, const char* file, u32 line) override
    {
        BX_UNUSED(file);
        BX_UNUSED(line);

        ASSERT(!ptr || owns(ptr), "Invalid or not-owned pointer ptr.");
        ASSERT(size_ <= SIZE_MASK, "Maximum allocatable size exceeded.");

        u8* memory = nullptr;

        if (!size_)
        {
            if (ptr)
            {
                Block block = make_block(ptr);
                ASSERT(block.is_valid(), "Invalid memory block.");

                if (block.header == make_block(last).header)
                {
                    for (;;)
                    {
                        block = make_block(block.header->prev);
                        last  = block.data - buffer - sizeof(Header);
                        top   = block.data - buffer + block.size();

                        ASSERT(top >= sizeof(Header),
                            "Stack allocator's top underflowed.");

                        if (block.is_valid())
                        {
                            break;
                        }
                    }
                }
                else
                {
                    block.invalidate();
                }
            }
        }
        else if (!ptr)
        {
            if (size_)
            {
                Block block = next_block(align);

                if (block.data + size_ <= buffer + size)
                {
                    block.reset(last, size_);

                    last   = block.data - buffer - sizeof(Header);
                    top    = block.data - buffer + size_;
                    memory = block.data;
                }
            }
        }
        else
        {
            Block block = make_block(ptr);

            if (block.header == make_block(last).header)
            {
                // TODO : Multiples of the previous alignment wouldn't matter either.
                ASSERT(!align || bx::isAligned(uintptr_t(block.data), align),
                    "Different (larger) data alignment for reallocation.");
                ASSERT(block.is_valid(), "Invalid memory block.");

                if (block.data + size_ <= buffer + size)
                {
                    block.reset(block.header->prev, size_);

                    top    = block.data - buffer + size_;
                    memory = block.data;
                }
            }
            else
            {
                memory = reinterpret_cast<u8*>(realloc(nullptr, size_, align, file, line));

                if (memory)
                {
                    bx::memCopy(memory, ptr, size_);

                    block.invalidate();
                }
            }
        }

        return memory;
    }

    Block make_block(void* data_ptr)
    {
        Block block;
        block.header = reinterpret_cast<Header*>(data_ptr) - 1;
        block.data   = reinterpret_cast<u8*>(data_ptr);

        return block;
    }

    Block make_block(u32 header_offset)
    {
        return make_block(buffer + header_offset + sizeof(Header));
    }

    Block next_block(size_t align)
    {
        void* data = bx::alignPtr(
            buffer + top,
            sizeof(Header),
            bx::max(align, std::alignment_of<Header>::value)
        );

        return make_block(data);
    }
};

void init(StackAllocator& allocator, void* buffer, u32 size)
{
    ASSERT(buffer, "Invalid buffer pointer.");
    ASSERT(size >= 64, "Too small buffer size %" PRIu32".", size);
    ASSERT(size <= StackAllocator::SIZE_MASK, "Too big buffer size %" PRIu32".", size);

    allocator.buffer = reinterpret_cast<u8*>(buffer);
    allocator.size   = size;
    allocator.top    = 0;
    allocator.last   = 0;

    StackAllocator::Block block = allocator.next_block(0);
    block.reset(0, 0);

    allocator.top = block.data - allocator.buffer;
}

TEST_CASE("Stack Allocator")
{
    u64 buffer[16];

    StackAllocator allocator;
    init(allocator, buffer, sizeof(buffer));
    TEST_REQUIRE(allocator.size == sizeof(buffer));
    TEST_REQUIRE(allocator.top == 8);
    TEST_REQUIRE(allocator.last == 0);

    void* first = BX_ALLOC(&allocator, 16);
    TEST_REQUIRE(first != nullptr);
    TEST_REQUIRE(allocator.owns(first));
    TEST_REQUIRE(allocator.size == sizeof(buffer));
    TEST_REQUIRE(allocator.top == 32);
    TEST_REQUIRE(allocator.last == 8);

    void* second = BX_ALLOC(&allocator, 8);
    TEST_REQUIRE(second != nullptr);
    TEST_REQUIRE(allocator.owns(second));
    TEST_REQUIRE(allocator.top == 48);
    TEST_REQUIRE(allocator.last == 32);

    void* third = BX_ALLOC(&allocator, 128);
    TEST_REQUIRE(third == nullptr);
    TEST_REQUIRE(!allocator.owns(third));
    TEST_REQUIRE(allocator.top == 48);
    TEST_REQUIRE(allocator.last == 32);

    BX_FREE(&allocator, third);
    TEST_REQUIRE(allocator.top == 48);
    TEST_REQUIRE(allocator.last == 32);

    void* second_realloced = BX_REALLOC(&allocator, second, 16);
    TEST_REQUIRE(second == second_realloced);
    TEST_REQUIRE(allocator.owns(second_realloced));
    TEST_REQUIRE(allocator.top == 56);
    TEST_REQUIRE(allocator.last == 32);

    void* first_realloced = BX_REALLOC(&allocator, first, 8);
    TEST_REQUIRE(first != first_realloced);
    TEST_REQUIRE(allocator.owns(first_realloced));
    TEST_REQUIRE(allocator.top == 72);
    TEST_REQUIRE(allocator.last == 56);

    BX_FREE(&allocator, second_realloced);
    TEST_REQUIRE(allocator.top == 72);
    TEST_REQUIRE(allocator.last == 56);

    BX_FREE(&allocator, first_realloced);
    TEST_REQUIRE(allocator.top == 8);
    TEST_REQUIRE(allocator.last == 0);
}


// -----------------------------------------------------------------------------
// SPAN
// -----------------------------------------------------------------------------

template <typename T>
struct Span
{
    T*  data;
    u32 size;

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

    operator Span<T>() const
    {
        return { data, Size };
    }
};

template <typename T, u32 Size>
void fill(FixedArray<T, Size>& array, const T& value)
{
    fill_value(array.data, value, Size);
}


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

    operator Span<T>() const
    {
        return { data, size };
    }
};

template <typename T>
void init(DynamicArray<T>& array, Allocator* allocator)
{
    ASSERT(!array.size, "Array not empty.")
    ASSERT(allocator, "Invalid allocator pointer.");

    array = {};
    array.allocator = allocator;
}

template <typename T>
void clear(DynamicArray<T>& array)
{
    ASSERT(array.allocator, "Invalid allocator pointer.");

    BX_ALIGNED_FREE(array.allocator, array.data, std::alignment_of<T>::value);

    array.data     = nullptr;
    array.size     = 0;
    array.capacity = 0;
}

template <typename T>
void deinit(DynamicArray<T>& array)
{
    clear(array);

    array.allocator = nullptr;
}

u32 capacity_hint(u32 capacity, u32 requested_size)
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

void append(DynamicArray<u8>& array, const void* data, u32 size)
{
    resize(array, array.size + size);

    bx::memCopy(array.data + array.size - size, data, size);
}

template <typename T>
T& pop(DynamicArray<T>& array)
{
    ASSERT(array.size, "Cannot pop from an empty array.");

    return array.data[--array.size];
}

TEST_CASE("Dynamic Array")
{
    CrtAllocator allocator;

    DynamicArray<int> array = {};
    init(array, &allocator);
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

    deinit(array);
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
// MATRIX STACK
// -----------------------------------------------------------------------------

template <u32 Size>
using MatrixStack = FixedStack<Mat4, Size>;

template <u32 Size>
void reset(MatrixStack<Size>& stack)
{
    reset(stack, HMM_Mat4d(1.0f));
}

template <u32 Size>
void multiply_top(MatrixStack<Size>& stack, const Mat4& matrix)
{
    stack.top = matrix * stack.top;
}


// -----------------------------------------------------------------------------
// WINDOW
// -----------------------------------------------------------------------------

struct WindowInfo
{
    Vec2i framebuffer_size;
    Vec2  invariant_size;
    Vec2  position_scale;
    Vec2  display_scale;
    f32   display_aspect;
    bool  display_scale_changed;
};

void update_window_info(GLFWwindow* window, WindowInfo& info)
{
    ASSERT(window, "Invalid window pointer.");

    Vec2i window_size;
    glfwGetWindowSize(window, &window_size[0], &window_size[1]);

    glfwGetFramebufferSize(window, &info.framebuffer_size[0], &info.framebuffer_size[1]);
    info.display_aspect = f32(info.framebuffer_size[0]) / info.framebuffer_size[1];

    const f32 prev_display_scale = info.display_scale[0];
    glfwGetWindowContentScale(window, &info.display_scale[0], &info.display_scale[1]);

    info.display_scale_changed = prev_display_scale != info.display_scale[0];

    for (i32 i = 0; i < 2; i++)
    {
        if (info.display_scale[i] != 1.0 &&
            window_size[i] * info.display_scale[i] != f32(info.framebuffer_size[i]))
        {
            info.invariant_size[i] = info.framebuffer_size[i] / info.display_scale[i];
            info.position_scale[i] = 1.0f / info.display_scale[i];
        }
        else
        {
            info.invariant_size[i] = f32(window_size[i]);
            info.position_scale[i] = 1.0f;
        }
    }
}

void resize_window(GLFWwindow* window, i32 width, i32 height, i32 flags)
{
    ASSERT(window, "Invalid window pointer.");
    ASSERT(flags >= 0, "Invalid window flags.");

    GLFWmonitor* monitor = glfwGetWindowMonitor(window);

    if (flags & WINDOW_FULL_SCREEN)
    {
        if (!monitor)
        {
            monitor = glfwGetPrimaryMonitor();
        }

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        if (width  <= 0) { width  = mode->width ; }
        if (height <= 0) { height = mode->height; }

        glfwSetWindowMonitor(window, monitor, 0, 0, width, height, GLFW_DONT_CARE);
    }
    else if (monitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        if (width  <= MIN_WINDOW_SIZE) { width  = DEFAULT_WINDOW_WIDTH ; }
        if (height <= MIN_WINDOW_SIZE) { height = DEFAULT_WINDOW_HEIGHT; }

        const i32 x = (mode->width  - width ) / 2;
        const i32 y = (mode->height - height) / 2;

        monitor = nullptr;

        glfwSetWindowMonitor(window, nullptr, x, y, width, height, GLFW_DONT_CARE);
    }

    // Other window aspects are ignored, if currently in the full screen mode.
    if (monitor)
    {
        return;
    }

    if (width  <= MIN_WINDOW_SIZE) { width  = DEFAULT_WINDOW_WIDTH ; }
    if (height <= MIN_WINDOW_SIZE) { height = DEFAULT_WINDOW_HEIGHT; }

    glfwSetWindowSize(window, width, height);

    if (flags & WINDOW_FIXED_ASPECT)
    {
        glfwSetWindowAspectRatio(window, width, height);
    }
    else
    {
        glfwSetWindowAspectRatio(window, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }

    const i32 resizable = (flags & WINDOW_FIXED_SIZE) ? GLFW_FALSE : GLFW_TRUE;
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, resizable);
}


// -----------------------------------------------------------------------------
// INPUT
// -----------------------------------------------------------------------------

enum struct InputState : u8
{
    DOWN     = 0x01,
    UP       = 0x02,
    HELD     = 0x04,
    REPEATED = 0x08,
};

template <typename T, u16 Size>
struct InputCache
{
    static constexpr u16 INPUT_SIZE    = Size + 1;
    static constexpr u16 INVALID_INPUT = Size;

    FixedArray<u8 , INPUT_SIZE> states;
    FixedArray<f32, INPUT_SIZE> timestamps;

    bool is(u16 input, InputState state) const
    {
        input = T::translate_input(input);

        return states[input] & u8(state);
    }

    f32 held_time(u16 input, f32 timestamp) const
    {
        input = T::translate_input(input);

        if (states[input] & (u8(InputState::DOWN) | u8(InputState::HELD)))
        {
            ASSERT(timestamp >= timestamps[input],
                "New timestamp %f older than the previous one %f.",
                timestamp, timestamps[input]);

            return timestamp -  timestamps[input];
        }

        return -1.0f;
    }

    void update_input(u16 input, InputState state, f32 timestamp = 0.0f)
    {
        states[input] |= u8(state);

        if (state == InputState::DOWN)
        {
            timestamps[input] = timestamp;
        }
    }

    void update_states()
    {
        for (u16 i = 0; i < INPUT_SIZE; i++)
        {
            if (states[i] & u8(InputState::UP))
            {
                states[i] = 0;
            }
            else if (states[i] & u8(InputState::DOWN))
            {
                states[i] = u8(InputState::HELD);
            }
            else
            {
                states[i] &= ~u8(InputState::REPEATED);
            }
        }
    }
};

struct KeyboardInput : InputCache<KeyboardInput, GLFW_KEY_LAST>
{
    static u16 translate_input(u16 input)
    {
        static const u16 s_keys[] =
        {
            0,                     // KEY_ANY

            GLFW_KEY_LEFT_ALT,     // KEY_ALT_LEFT
            GLFW_KEY_RIGHT_ALT,    // KEY_ALT_RIGHT
            GLFW_KEY_BACKSPACE,    // KEY_BACKSPACE
            GLFW_KEY_LEFT_CONTROL, // KEY_CONTROL_LEFT
            GLFW_KEY_RIGHT_CONTROL, // KEY_CONTROL_RIGHT
            GLFW_KEY_DELETE,        // KEY_DELETE
            GLFW_KEY_DOWN,          // KEY_DOWN
            GLFW_KEY_ENTER,         // KEY_ENTER
            GLFW_KEY_ESCAPE,        // KEY_ESCAPE
            GLFW_KEY_LEFT,          // KEY_LEFT
            GLFW_KEY_RIGHT,         // KEY_RIGHT
            GLFW_KEY_LEFT_SHIFT,    // KEY_SHIFT_LEFT
            GLFW_KEY_RIGHT_SHIFT,   // KEY_SHIFT_RIGHT
            GLFW_KEY_SPACE,         // KEY_SPACE
            GLFW_KEY_LEFT_SUPER,    // KEY_SUPER_LEFT
            GLFW_KEY_RIGHT_SUPER,   // KEY_SUPER_RIGHT
            GLFW_KEY_TAB,           // KEY_TAB
            GLFW_KEY_UP,            // KEY_UP

            GLFW_KEY_F1,            // KEY_F1
            GLFW_KEY_F2,            // KEY_F2
            GLFW_KEY_F3,            // KEY_F3
            GLFW_KEY_F4,            // KEY_F4
            GLFW_KEY_F5,            // KEY_F5
            GLFW_KEY_F6,            // KEY_F6
            GLFW_KEY_F7,            // KEY_F7
            GLFW_KEY_F8,            // KEY_F8
            GLFW_KEY_F9,            // KEY_F9
            GLFW_KEY_F10,           // KEY_F10
            GLFW_KEY_F11,           // KEY_F11
            GLFW_KEY_F12,           // KEY_F12
        };

        if (input < int(BX_COUNTOF(s_keys)))
        {
            input = s_keys[input];
        }
        else if ((input >= 'A' && input <= 'Z'))
        {
            input = input - 'A' + GLFW_KEY_A;
        }
        else if (input >= 'a' && input <= 'z')
        {
            input = input - 'a' + GLFW_KEY_A;
        }
        else
        {
            input = INVALID_INPUT;
        }

        return input;
    }
};

struct MouseInput : InputCache<MouseInput, GLFW_MOUSE_BUTTON_LAST>
{
    static constexpr f32 REPEATED_CLICK_DELAY = 0.5f; // NOTE : Could be configurable.

    Vec2                       current;
    Vec2                       previous;
    Vec2                       delta;
    Vec2                       scroll;
    FixedArray<u8, INPUT_SIZE> clicks;

    u8 repeated_click_count(u16 input) const
    {
        input = translate_input(input);

        return (states[input] & u8(InputState::DOWN)) ? clicks[input] : 0;
    }

    void update_input(u16 input, InputState state, f32 timestamp = 0.0f)
    {
        states[input] |= u8(state);

        if (state == InputState::DOWN)
        {
            if (timestamp - timestamps[input] <= REPEATED_CLICK_DELAY)
            {
                clicks[input]++;
            }
            else
            {
                clicks[input] = 1;
            }

            timestamps[input] = timestamp;
        }
    }

    void update_position(GLFWwindow* window, const Vec2& scale)
    {
        f64 x, y;
        glfwGetCursorPos(window, &x, &y);

        current.X = f32(scale.X * x);
        current.Y = f32(scale.Y * y);
    }

    void update_position_delta()
    {
        delta    = current - previous;
        previous = current;
    }

    static u16 translate_input(u16 input)
    {
        switch (input)
        {
        case MOUSE_LEFT:
            return GLFW_MOUSE_BUTTON_LEFT;
        case MOUSE_RIGHT:
            return GLFW_MOUSE_BUTTON_RIGHT;
        case MOUSE_MIDDLE:
            return GLFW_MOUSE_BUTTON_MIDDLE;
        default:
            return INVALID_INPUT;
        }
    }
};


// -----------------------------------------------------------------------------
// BGFX ENUM REDUCTION UTILITY
// -----------------------------------------------------------------------------

template <typename EnumT, typename ValueT>
struct BgfxReducedEnum
{
    ValueT value;

    static_assert(
        std::is_unsigned<ValueT>::value,
        "Underlying value type must be unsigned integral type."
    );

    static_assert(
        sizeof(ValueT) < sizeof(typename EnumT::Enum),
        "Underlying value type must be smaller than the original enum type."
    );

    static_assert(
        EnumT::Count <= ValueT(-1),
        "Underlying value type has insufficient value range."
    );

    BgfxReducedEnum& operator=(typename EnumT::Enum value_)
    {
        value = ValueT(value_);
        return *this;
    }

    operator typename EnumT::Enum() const
    {
        return typename EnumT::Enum(value);
    }
};


// -----------------------------------------------------------------------------
// VERTEX LAYOUT
// -----------------------------------------------------------------------------

using BgfxAttrib     = BgfxReducedEnum<bgfx::Attrib    , u8>;
using BgfxAttribType = BgfxReducedEnum<bgfx::AttribType, u8>;

struct VertexLayoutCache
{
    FixedArray<bgfx::VertexLayout      , 128> layouts;
    FixedArray<bgfx::VertexLayoutHandle, 128> handles;
};

struct VertexLayoutAttrib
{
    u32            flag;
    BgfxAttrib     type;
    BgfxAttribType element_type;
    u8             element_count;
    u8             byte_size;
    bool           normalized;
    bool           packed;
};

const VertexLayoutAttrib s_vertex_layout_attribs[] =
{
    { VERTEX_POSITION    , {bgfx::Attrib::Position }, {bgfx::AttribType::Float}, 3, 0, false, false },
    { VERTEX_COLOR       , {bgfx::Attrib::Color0   }, {bgfx::AttribType::Uint8}, 4, 4, true , false },
    { VERTEX_NORMAL      , {bgfx::Attrib::Normal   }, {bgfx::AttribType::Uint8}, 4, 4, true , true  },
    { VERTEX_TEXCOORD    , {bgfx::Attrib::TexCoord0}, {bgfx::AttribType::Int16}, 2, 4, true , true  },
    { VERTEX_TEXCOORD_F32, {bgfx::Attrib::TexCoord0}, {bgfx::AttribType::Float}, 2, 8, false, false },
};

constexpr u32 vertex_layout_index(u32 attribs, u32 skips = 0)
{
    static_assert(
        VERTEX_ATTRIB_MASK  >>  VERTEX_ATTRIB_SHIFT       == 0b0000111 &&
        (VERTEX_ATTRIB_MASK >> (VERTEX_ATTRIB_SHIFT - 3)) == 0b0111000 &&
        TEXCOORD_F32        >>  6                         == 0b1000000,
        "Invalid index assumptions in `vertex_layout_index`."
    );

    return
        ((skips   & VERTEX_ATTRIB_MASK) >>  VERTEX_ATTRIB_SHIFT     ) | // Bits 0..2.
        ((attribs & VERTEX_ATTRIB_MASK) >> (VERTEX_ATTRIB_SHIFT - 3)) | // Bits 3..5.
        ((attribs & TEXCOORD_F32      ) >>  6                       ) ; // Bit 6.
}

void add_vertex_layout(VertexLayoutCache& cache, u32 attribs, u32 skips)
{
    ASSERT((attribs & skips) == 0, "`Attribute and skip flags must be disjoint.");

    bgfx::VertexLayout layout;
    layout.begin();

    for (u32 i = 0; i < BX_COUNTOF(s_vertex_layout_attribs); i++)
    {
        const VertexLayoutAttrib& attrib = s_vertex_layout_attribs[i];

        if ((attribs & attrib.flag) == attrib.flag)
        {
            layout.add(
                attrib.type,
                attrib.element_count,
                attrib.element_type,
                attrib.normalized,
                attrib.packed
            );
        }
        else if ((skips & attrib.flag) == attrib.flag)
        {
            layout.skip(attrib.byte_size);
        }
    }

    layout.end();
    ASSERT(layout.getStride() % 4 == 0, "Layout stride must be multiple of 4 bytes.");

    const u32 index = vertex_layout_index(attribs, skips);
    ASSERT(!bgfx::isValid(cache.handles[index]), "Cannot reset a valid layout.");

    cache.layouts[index] = layout;
    cache.handles[index] = bgfx::createVertexLayout(layout);
}

void init(VertexLayoutCache& cache)
{
    fill(cache.handles, BGFX_INVALID_HANDLE);

    add_vertex_layout(cache, VERTEX_POSITION, 0);

    for (u32 attrib_mask = 1; attrib_mask < 16; attrib_mask++)
    {
        if ((attrib_mask & 0xc) == 0xc)
        {
            // Exclude mixing `VERTEX_TEXCOORD` and `VERTEX_TEXCOORD_F32`.
            continue;
        }

        u32 attribs = 0;

        for (u32 i = 1; i < BX_COUNTOF(s_vertex_layout_attribs); i++)
        {
            if (attrib_mask & (1 << (i - 1)))
            {
                attribs |= s_vertex_layout_attribs[i].flag;
            }
        }

        add_vertex_layout(cache, attribs, 0);

        if (bx::isPowerOf2(attrib_mask))
        {
            continue;
        }

        // Add variants with skipped attributes (for aliasing).
        for (u32 skip_mask = 1; skip_mask < 16; skip_mask++)
        {
            const u32 skipped_attribs = attrib_mask & skip_mask;

            if (skipped_attribs == skip_mask)
            {
                u32 skips = 0;

                for (u32 i = 1; i < BX_COUNTOF(s_vertex_layout_attribs); i++)
                {
                    if (skip_mask & (1 << (i - 1)))
                    {
                        skips |= s_vertex_layout_attribs[i].flag;
                    }
                }

                add_vertex_layout(cache, attribs, skips);
            }
        }
    }
}

void deinit(VertexLayoutCache& cache)
{
    for (u32 i = 0; i < cache.handles.size; i++)
    {
        destroy_if_valid(cache.handles[i]);
    }
}


// -----------------------------------------------------------------------------
// VERTEX ATTRIBUTE STATE
// -----------------------------------------------------------------------------

struct VertexAttribState;

using PackedColor    = u32; // As RGBA_u8.
using PackedNormal   = u32; // As RGB_u8.
using PackedTexcoord = u32; // As RG_s16.
using FullTexcoord   = Vec2;

using ColorStoreFunc    = void (*)(VertexAttribState&, u32);
using NormalStoreFunc   = void (*)(VertexAttribState&, f32, f32, f32);
using TexcoordStoreFunc = void (*)(VertexAttribState&, f32, f32);

BX_ALIGN_DECL_16(struct) VertexAttribState
{
    u8                data[32];
    u32               size;

    PackedColor*      packed_color;
    PackedNormal*     packed_normal;
    PackedTexcoord*   packed_texcoord;
    FullTexcoord*     full_texcoord;

    ColorStoreFunc    store_color;
    NormalStoreFunc   store_normal;
    TexcoordStoreFunc store_texcoord;
};

void store_packed_color(VertexAttribState& state, u32 rgba)
{
    *state.packed_color = bx::endianSwap(rgba);
}

void store_packed_normal(VertexAttribState& state, f32 x, f32 y, f32 z)
{
    const f32 normalized[] =
    {
        x * 0.5f + 0.5f,
        y * 0.5f + 0.5f,
        z * 0.5f + 0.5f,
    };

    bx::packRgb8(state.packed_normal, normalized);
}

void store_packed_texcoord(VertexAttribState& state, f32 u, f32 v)
{
    const f32 elems[] = { u, v };

    bx::packRg16S(state.packed_texcoord, elems);
}

void store_full_texcoord(VertexAttribState& state, f32 u, f32 v)
{
    *state.full_texcoord = HMM_Vec2(u, v);
}

void store_no_color(VertexAttribState&, u32)
{
}

void store_no_normal(VertexAttribState&, f32, f32, f32)
{
}

void store_no_texcoord(VertexAttribState&, f32, f32)
{
}

template <typename T>
constexpr T* vertex_attrib(VertexAttribState& state, u32 offset)
{
    static_assert(is_pod<T>(),
        "`T` must be POD type.");

    ASSERT(offset % std::alignment_of<T>::value == 0,
        "Offset %" PRIu32 " not multiple of alignment of return type.");

    ASSERT(offset + sizeof(T) <= sizeof(state.data),
        "Requested data go beyond vertex vertex_attrib state's memory.");

    return reinterpret_cast<T*>(state.data + offset);
}

void reset(VertexAttribState& state, u32 flags)
{
    static_assert(
        VERTEX_COLOR  < VERTEX_NORMAL   &&
        VERTEX_NORMAL < VERTEX_TEXCOORD &&
        VERTEX_NORMAL < VERTEX_TEXCOORD_F32,
        "Vertex attributes' order assumption violated."
    );

    bx::memSet(&state, 0, sizeof(state));

    state.store_color    = store_no_color;
    state.store_normal   = store_no_normal;
    state.store_texcoord = store_no_texcoord;

    if (flags & VERTEX_COLOR)
    {
        state.packed_color = vertex_attrib<PackedColor>(state, state.size);
        state.store_color  = store_packed_color;
        state.size        += sizeof(PackedColor);
    }

    if (flags & VERTEX_NORMAL)
    {
        state.packed_normal = vertex_attrib<PackedNormal>(state, state.size);
        state.store_normal  = store_packed_normal;
        state.size         += sizeof(PackedNormal);
    }

    // NOTE : `VERTEX_TEXCOORD_F32` has two bits on.
    if ((flags & VERTEX_TEXCOORD_F32) == VERTEX_TEXCOORD_F32)
    {
        state.full_texcoord  = vertex_attrib<FullTexcoord>(state, state.size);
        state.store_texcoord = store_full_texcoord;
        state.size          += sizeof(FullTexcoord);
    }

    else if (flags & VERTEX_TEXCOORD)
    {
        
        state.packed_texcoord = vertex_attrib<PackedTexcoord>(state, state.size);
        state.store_texcoord  = store_full_texcoord;
        state.size           += sizeof(PackedTexcoord);
    }
}

TEST_CASE("Vertex Attribute State")
{
    // ...
}


// -----------------------------------------------------------------------------
// GENERIC RECORDING INFO
// -----------------------------------------------------------------------------

struct RecordInfo
{
    u32  flags;
    u32  extra_data;
    u16  id;
    bool is_transform;
};


// -----------------------------------------------------------------------------
// VERTEX SUBMISSION (I / II)
// -----------------------------------------------------------------------------

struct MeshRecorder;

using VertexStoreFunc = void (*)(const Vec3&, const VertexAttribState&, MeshRecorder&);

void reset(VertexStoreFunc& func, u32 flags);


// -----------------------------------------------------------------------------
// MESH RECORDING
// -----------------------------------------------------------------------------

struct MeshRecorder
{
    DynamicArray<u8>  attrib_buffer;
    DynamicArray<u8>  position_buffer;
    VertexAttribState attrib_state;
    VertexStoreFunc   store_vertex;
    u32               vertex_count;
    u32               invocation_count;
};

void init(MeshRecorder& recorder, Allocator* allocator)
{
    recorder = {};

    init(recorder.attrib_buffer  , allocator);
    init(recorder.position_buffer, allocator);
}

void start(MeshRecorder& recorder, u32 flags)
{
    reset(recorder.attrib_state, flags);
    reset(recorder.store_vertex, flags);

    reserve(recorder.attrib_buffer  , 32_kB * recorder.attrib_state.size);
    reserve(recorder.position_buffer, 32_kB * sizeof(float) * 3);

    recorder.vertex_count     = 0;
    recorder.invocation_count = 0;
}

void end(MeshRecorder& recorder)
{
    reset(recorder.attrib_state, 0);

    clear(recorder.attrib_buffer  );
    clear(recorder.position_buffer);

    recorder.store_vertex     = nullptr;
    recorder.vertex_count     = 0;
    recorder.invocation_count = 0;
}


// -----------------------------------------------------------------------------
// VERTEX SUBMISSION (II / II)
// -----------------------------------------------------------------------------

void emulate_quad(DynamicArray<u8>& buffer, u32 vertex_size)
{
    ASSERT(vertex_size > 0,
        "Zero vertex size.");
    ASSERT(buffer.size > 0,
        "Empty vertex buffer.");
    ASSERT(buffer.size % vertex_size == 0,
        "Buffer size " PRIu32 " not divisible by vertex size " PRIu32 ".",
        buffer.size, size);
    ASSERT((buffer.size / vertex_size) % 3 == 0,
        "Quad emulation should be done with 3 outstanding vertices, but got " PRIu32 ".",
        (buffer.size / vertex_size));

    resize(buffer, buffer.size + 2 * vertex_size);

    u8* end = buffer.data + buffer.size;

    // Assuming the last triangle has relative indices
    // [v0, v1, v2] = [-5, -4, -3], we need to copy the vertices v0 and v2.
    bx::memCopy(end - 2 * vertex_size, end - 5 * vertex_size, vertex_size);
    bx::memCopy(end -     vertex_size, end - 3 * vertex_size, vertex_size);
}

template <bool IsQuadMesh, bool HasAttribs>
void store_vertex(const Vec3& position, const VertexAttribState& attrib_state, MeshRecorder& recorder)
{
    if constexpr (IsQuadMesh)
    {
        if ((recorder.invocation_count & 3) == 3)
        {
            emulate_quad(recorder.position_buffer, sizeof(position));

            if constexpr (HasAttribs)
            {
                emulate_quad(recorder.attrib_buffer, attrib_state.size);
            }

            recorder.vertex_count += 2;
        }

        recorder.invocation_count++;
    }

    recorder.vertex_count++;

    append(recorder.position_buffer, &position, sizeof(position));

    if constexpr (HasAttribs)
    {
        append(recorder.attrib_buffer, attrib_state.data, attrib_state.size);
    }
}

const VertexStoreFunc s_vertex_store_funcs[] =
{
    store_vertex<0, 0>,
    store_vertex<0, 1>,
    store_vertex<1, 0>,
    store_vertex<1, 1>,
};

void reset(VertexStoreFunc& func, u32 flags)
{
    const bool is_quad_mesh = flags & PRIMITIVE_QUADS;
    const bool has_attribs  = flags & VERTEX_ATTRIB_MASK;

    func = s_vertex_store_funcs[is_quad_mesh * 2 + has_attribs];
}


// -----------------------------------------------------------------------------
// VERTEX / INDEX BUFFER CREATION
// -----------------------------------------------------------------------------

union VertexBufferUnion
{
    u16                             raw_index;
    u16                             transient_index;
    bgfx::VertexBufferHandle        static_buffer;
    bgfx::DynamicVertexBufferHandle dynamic_buffer;
};

union IndexBufferUnion
{
    u16                            raw_index;
    bgfx::IndexBufferHandle        static_buffer;
    bgfx::DynamicIndexBufferHandle dynamic_buffer;
};

VertexBufferUnion create_persistent_vertex_buffer
(
    u16                       type,
    const meshopt_Stream&     stream,
    const bgfx::VertexLayout& layout,
    u32                       vertex_count,
    u32                       remapped_vertex_count,
    const u32*                remap_table,
    void**                    remapped_memory = nullptr
)
{
    ASSERT(type == MESH_STATIC || type == MESH_DYNAMIC, "Invalid mesh type.");
    ASSERT(remap_table, "Invalid remapping table.");

    // TODO : This should use some scratch / frame memory.
    const bgfx::Memory* memory = bgfx::alloc(u32(remapped_vertex_count * stream.size));
    ASSERT(memory && memory->data, "Invalid BGFX-created memory.");

    meshopt_remapVertexBuffer(memory->data, stream.data, vertex_count, stream.size, remap_table);

    if (remapped_memory)
    {
        *remapped_memory = memory->data;
    }

    u16 handle = bgfx::kInvalidHandle;

    switch (type)
    {
    case MESH_STATIC:
        handle = bgfx::createVertexBuffer(memory, layout).idx;
        break;
    case MESH_DYNAMIC:
        handle = bgfx::createDynamicVertexBuffer(memory, layout).idx;
        break;
    }

    WARN(handle != bgfx::kInvalidHandle, "Vertex buffer creation failed.");

    return { handle };
}

IndexBufferUnion create_persistent_index_buffer
(
    u16        type,
    u32        vertex_count,
    u32        indexed_vertex_count,
    const f32* vertex_positions,
    const u32* remap_table,
    bool       optimize
)
{
    ASSERT(type == MESH_STATIC || type == MESH_DYNAMIC, "Invalid mesh type.");
    ASSERT(remap_table, "Invalid remapping table.");

    u16 buffer_flags = BGFX_BUFFER_NONE;
    u32 type_size    = sizeof(u16);

    if (indexed_vertex_count > U16_MAX)
    {
        buffer_flags = BGFX_BUFFER_INDEX32;
        type_size    = sizeof(u32);
    }

    // meshoptimizer works only with `u32`, so we allocate the memory for it
    // anyway, to avoid doing an additional copy.
    // TODO : This should use some scratch / frame memory.
    const bgfx::Memory* memory = bgfx::alloc(vertex_count * sizeof(u32));
    ASSERT(memory && memory->data, "Invalid BGFX-created memory.");

    u32* indices = reinterpret_cast<u32*>(memory->data);

    meshopt_remapIndexBuffer(indices, nullptr, vertex_count, remap_table);

    if (optimize && vertex_positions)
    {
        meshopt_optimizeVertexCache(indices, indices, vertex_count,
            indexed_vertex_count);

        meshopt_optimizeOverdraw(indices, indices, vertex_count,
            vertex_positions, indexed_vertex_count, 3 * sizeof(f32), 1.05f);

        // TODO : Consider also doing `meshopt_optimizeVertexFetch`?
    }

    if (type_size == sizeof(u16))
    {
        const u32* src = reinterpret_cast<u32*>(memory->data);
        u16*       dst = reinterpret_cast<u16*>(memory->data);

        for (u32 i = 0; i < vertex_count; i++)
        {
            dst[i] = src[i];
        }

        const_cast<bgfx::Memory*>(memory)->size /= 2;
    }

    u16 handle = bgfx::kInvalidHandle;

    switch (type)
    {
    case MESH_STATIC:
        handle = bgfx::createIndexBuffer(memory, buffer_flags).idx;
        break;
    case MESH_DYNAMIC:
        handle = bgfx::createDynamicIndexBuffer(memory, buffer_flags).idx;
        break;
    }

    WARN(handle != bgfx::kInvalidHandle, "Index buffer creation failed.");

    return { handle };
}

bool create_transient_vertex_buffer
(
    const Span<u8>&              buffer,
    const bgfx::VertexLayout&    layout,
    bgfx::TransientVertexBuffer* tvb
)
{
    ASSERT(buffer.size, "Empty buffer.");
    ASSERT(layout.getStride(), "Zero layout stride.");
    ASSERT(buffer.size % layout.getStride() == 0,
        "Data / layout size mismatch; %" PRIu32 " not divisible by %" PRIu32 ".",
        buffer.size, layout.getStride()
    );

    const u32 count = buffer.size / layout.getStride();

    if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
    {
        return false;
    }

    bgfx::allocTransientVertexBuffer(tvb, count, layout);
    bx::memCopy(tvb->data, buffer.data, buffer.size);

    return true;
}


// -----------------------------------------------------------------------------
// MESH & MESH CACHING
// -----------------------------------------------------------------------------

struct Mesh
{
    u32               element_count;
    u32               extra_data;
    u32               flags;
    VertexBufferUnion positions;
    VertexBufferUnion attribs;
    IndexBufferUnion  indices;
};

struct MeshCache
{
    Mutex                                                          mutex;
    FixedArray<Mesh, MAX_MESHES>                                   meshes;
    FixedArray<u16, MAX_TRANSIENT_BUFFERS>                         transient_indices;
    FixedArray<bgfx::TransientVertexBuffer, MAX_TRANSIENT_BUFFERS> transient_buffers;
    u32                                                            transient_buffer_count;
    u32                                                            transient_memory_exhausted;
};

u16 mesh_type(u32 flags)
{
    constexpr u16 types[] =
    {
        MESH_STATIC,
        MESH_TRANSIENT,
        MESH_DYNAMIC,
        MESH_INVALID,
    };

    return types[(flags & MESH_TYPE_MASK) >> MESH_TYPE_SHIFT];
}

bool is_valid(const Mesh& mesh)
{
    // TODO : A more complete check might be in order (at least an assertion).
    return mesh.element_count != 0;
}

void init(Mesh& mesh)
{
    mesh.element_count       = 0;
    mesh.extra_data          = 0;
    mesh.flags               = 0;
    mesh.positions.raw_index = bgfx::kInvalidHandle;
    mesh.attribs  .raw_index = bgfx::kInvalidHandle;
    mesh.indices  .raw_index = bgfx::kInvalidHandle;
}

void destroy(Mesh& mesh)
{
    const u16 type = mesh_type(mesh.flags);

    if (type == MESH_STATIC)
    {
        destroy_if_valid(mesh.positions.static_buffer);
        destroy_if_valid(mesh.attribs  .static_buffer);
        destroy_if_valid(mesh.indices  .static_buffer);
    }
    else if (type == MESH_DYNAMIC)
    {
        destroy_if_valid(mesh.positions.dynamic_buffer);
        destroy_if_valid(mesh.attribs  .dynamic_buffer);
        destroy_if_valid(mesh.indices  .dynamic_buffer);
    }

    mesh = {};
}

bool create_persistent_geometry
(
    u32                        flags,
    u32                        count,
    const Span<u8>*            attribs,
    const bgfx::VertexLayout** layouts,
    Allocator*                 temp_allocator,
    VertexBufferUnion*         output_vertex_buffers,
    IndexBufferUnion&          output_index_buffer
)
{
    const u32 type = mesh_type(flags);
    const u32 vertex_count = attribs[0].size / layouts[0]->getStride();

    FixedArray<meshopt_Stream, 2> streams;
    ASSERT(streams.size >= count, "Insufficient stream array size.");

    for (u32 i = 0; i < count; i++)
    {
        ASSERT(vertex_count == attribs[i].size / layouts[i]->getStride(),
            "Mismatched number of vertices for attribute buffer %" PRIu32 ".", i
        );

        streams[i] = {
            attribs[i].data,
            layouts[i]->getStride(),
            layouts[i]->getStride()
        };
    }

    DynamicArray<u32> remap_table;
    init(remap_table, temp_allocator);
    defer(deinit(remap_table));

    resize(remap_table, vertex_count);

    const u32 indexed_vertex_count = count > 1
        ? u32(meshopt_generateVertexRemapMulti(
            remap_table.data, nullptr, vertex_count, vertex_count, streams.data,
            count
        ))
        : u32(meshopt_generateVertexRemap(
            remap_table.data, nullptr, vertex_count, streams[0].data,
            vertex_count, streams[0].size
        ));

    void* vertex_positions = nullptr;

    for (u32 i = 0; i < count; i++)
    {
        output_vertex_buffers[i] = create_persistent_vertex_buffer(
            type, streams[i], *layouts[i], vertex_count, indexed_vertex_count,
            remap_table.data, i ? nullptr : &vertex_positions
        );
    }

    const bool optimize_geometry =
         (flags & OPTIMIZE_GEOMETRY) &&
        ((flags & PRIMITIVE_TYPE_MASK) <= PRIMITIVE_QUADS);

    output_index_buffer = create_persistent_index_buffer(
        type, vertex_count, indexed_vertex_count,
        static_cast<f32*>(vertex_positions), remap_table.data, optimize_geometry
    );

    // TODO : Check that all the buffers were successfully created and perform
    //        the cleanup if not.

    return true;
}

bool create_transient_geometry
(
    u32                          count,
    const Span<u8>*              attribs,
    const bgfx::VertexLayout**   layouts,
    bgfx::TransientVertexBuffer* output_vertex_buffers
)
{
    for (u32 i = 0; i < count; i++)
    {
        if (!create_transient_vertex_buffer(
            attribs[i],
            *layouts[i],
            &output_vertex_buffers[i]
        ))
        {
            return false;
        }
    }

    return true;
}

void add_mesh
(
    MeshCache&                      cache,
    const RecordInfo&               info,
    const MeshRecorder&             recorder,
    const Span<bgfx::VertexLayout>& layouts_,
    Allocator*                      thread_local_temp_allocator
)
{
    ASSERT(info.id < cache.meshes.size,
        "Mesh id %" PRIu16 " out of bounds (%" PRIu32").",
        info.id, cache.meshes.size
    );

    const u16 type = mesh_type(info.flags);

    if (type == MESH_INVALID)
    {
        WARN(true, "Invalid registered mesh type.");
        return;
    }

    const u32 count = 1 + (recorder.attrib_buffer.size > 0);

    Span<u8>                  attribs[2];
    const bgfx::VertexLayout* layouts[2];

    attribs[0] = recorder.position_buffer;
    layouts[0] = &layouts_[vertex_layout_index(VERTEX_POSITION)];

    if (count > 1)
    {
        attribs[1] = recorder.attrib_buffer;
        layouts[1] = &layouts_[vertex_layout_index(info.flags)];
    }

    Mesh mesh;
    init(mesh);

    mesh.element_count = recorder.vertex_count;
    mesh.extra_data    = info.extra_data;
    mesh.flags         = info.flags;

    if (type != MESH_TRANSIENT)
    {
        static_assert(
            &mesh.positions + 1 == &mesh.attribs,
            "Invalid `Mesh` structure layout assumption."
        );

        if (!create_persistent_geometry(
            info.flags, count, attribs, layouts, thread_local_temp_allocator,
            &mesh.positions, mesh.indices
        ))
        {
            WARN(true, "Failed to create %s mesh with ID %" PRIu16 ".",
                type == MESH_STATIC ? "static" : "dynamic", info.id
            );

            return;
        }
    }
    else if (0 == bx::atomicCompareAndSwap(&cache.transient_memory_exhausted, 0u, 0u))
    {
        const u32 offset = bx::atomicFetchAndAdd(&cache.transient_buffer_count, count);

        if (offset + count > cache.transient_buffers.size)
        {
            // TODO : This should be a once-per-frame warning.
            WARN(true, "Transient buffer count limit %" PRIu32 " exceeded.",
                cache.transient_buffers.size
            );

            return;
        }

        bool success;
        {
            // NOTE : Mutexing since it seems that both
            //        `getAvailTransientVertexBuffer` and
            //        `allocTransientVertexBuffer` aren't thread safe.
            MutexScope lock(cache.mutex);

            success = create_transient_geometry(
                count, attribs, layouts, &cache.transient_buffers[offset]
            );
        }

        if (!success)
        {
            WARN(true, "Transient memory of %" PRIu32 " MB exhausted.",
                0 // TODO : Provide the actual limit.
            );

            bx::atomicCompareAndSwap(&cache.transient_memory_exhausted, 0u, 1u);

            return;
        }

        mesh.positions.transient_index = u16(offset);

        if (count > 1)
        {
            mesh.attribs.transient_index = u16(offset + 1);
        }
    }

    {
        MutexScope lock(cache.mutex);

        destroy(cache.meshes[info.id]);

        cache.meshes[info.id] = mesh;
    }
}

void init(MeshCache& cache)
{
    for (u32 i = 0; i < cache.meshes.size; i++)
    {
        init(cache.meshes[i]);
    }
}

void deinit(MeshCache& cache)
{
    for (u32 i = 0; i < cache.meshes.size; i++)
    {
        destroy(cache.meshes[i]);
    }
}

void init_frame(MeshCache& cache)
{
    MutexScope lock(cache.mutex);

    cache.transient_buffer_count     = 0;
    cache.transient_memory_exhausted = 0;
}


// -----------------------------------------------------------------------------
// TEXTURE & TEXTURE CACHING
// -----------------------------------------------------------------------------

using BgfxTextureFormat   = BgfxReducedEnum<bgfx::TextureFormat  , u8>;
using BgfxBackbufferRatio = BgfxReducedEnum<bgfx::BackbufferRatio, u8>;

struct Texture
{
    bgfx::TextureHandle handle;
    u16                 width;
    u16                 height;
    BgfxTextureFormat   format;
    BgfxBackbufferRatio ratio;
    bgfx::TextureHandle blit_handle;
    u32                 read_frame;
};

void init(Texture& texture)
{
    texture.handle      = BGFX_INVALID_HANDLE;
    texture.width       = 0;
    texture.height      = 0;
    texture.format      = bgfx::TextureFormat  ::Count;
    texture.ratio       = bgfx::BackbufferRatio::Count;
    texture.blit_handle = BGFX_INVALID_HANDLE;
    texture.read_frame  = U32_MAX;
}

void destroy(Texture& texture)
{
    ASSERT(bgfx::isValid(texture.blit_handle) <= bgfx::isValid(texture.handle),
        "Blit handle %" PRIu16 " valid, but the main one is not.",
        texture.blit_handle
    );

    destroy_if_valid(texture.handle);
    destroy_if_valid(texture.blit_handle);

    texture = {};
}


// -----------------------------------------------------------------------------
// INSTANCE RECORDING
// -----------------------------------------------------------------------------

struct InstanceRecorder
{
    DynamicArray<u8> buffer;
    u16              instance_size;
};

void init(InstanceRecorder& recorder, Allocator* allocator)
{
    recorder = {};

    init(recorder.buffer, allocator);
}

void start(InstanceRecorder& recorder, u32 type)
{
    constexpr u32 type_sizes[] =
    {
        sizeof(Mat4), // INSTANCE_TRANSFORM
        16,           // INSTANCE_DATA_16
        32,           // INSTANCE_DATA_32
        48,           // INSTANCE_DATA_48
        64,           // INSTANCE_DATA_64
        80,           // INSTANCE_DATA_80
        96,           // INSTANCE_DATA_96
        112,          // INSTANCE_DATA_112
    };

    reserve(recorder.buffer, bx::min(4_MB, 2048u * recorder.instance_size));

    recorder.instance_size = type_sizes[bx::max<u32>(type, BX_COUNTOF(type_sizes) - 1)];
}

void end(InstanceRecorder& recorder)
{
    clear(recorder.buffer);
    recorder.instance_size = 0;
}

void append(InstanceRecorder& recorder, const void* instance_data)
{
    ASSERT(instance_data, "Invalid `instance_data` pointer.");

    append(recorder.buffer, instance_data, recorder.instance_size);
}

u32 instance_count(const InstanceRecorder& recorder)
{
    return recorder.buffer.size / recorder.instance_size;
}


// -----------------------------------------------------------------------------
// INSTANCE & INSTANCE CACHE
// -----------------------------------------------------------------------------

struct InstanceData
{
    bgfx::InstanceDataBuffer buffer;
    bool                     is_transform;
};

struct InstanceCache
{
    Mutex                                          mutex;
    FixedArray<InstanceData, MAX_INSTANCE_BUFFERS> data;
};

void add_instances
(
    InstanceCache&          cache,
    const InstanceRecorder& recorder,
    u16                     id,
    bool                    is_transform
)
{
    ASSERT(id < cache.data.size,
        "Mesh id %" PRIu16 " out of bounds (%" PRIu32").",
        id, cache.data.size
    );

    const u32 count  = instance_count(recorder);
    const u16 stride = recorder.instance_size;

    // NOTE : Mutexing since it seems that both `getAvailInstanceDataBuffer`
    //        and `allocInstanceDataBuffer` aren't thread safe.
    MutexScope lock(cache.mutex);

    if (bgfx::getAvailInstanceDataBuffer(count, stride) < count)
    {
        WARN(true, "Instance buffer memory exhausted.");
        return;
    }

    InstanceData &instance_data = cache.data[id];

    instance_data.is_transform = is_transform;

    bgfx::allocInstanceDataBuffer(&instance_data.buffer, count, stride);
    bx::memCopy(instance_data.buffer.data, recorder.buffer.data,
        recorder.buffer.size
    );

}


// -----------------------------------------------------------------------------

} // unnamed namespace

} // namespace rwr

} // namespace mnm