#include <mnm/mnm.h>

#include <inttypes.h>             // PRI*
#include <math.h>                 // acosf
#include <stddef.h>               // offsetof, size_t
#include <stdint.h>               // *int*_t, ptrdiff_t, UINT*_MAX, uintptr_t

#include <thread>                 // hardware_concurrency
#include <type_traits>            // alignment_of, is_standard_layout, is_trivial, is_trivially_copyable, is_unsigned

#include <bx/allocator.h>         // alignPtr, AllocatorI, BX_ALIGNED_*
#include <bx/bx.h>                // BX_ASSERT, BX_CONCATENATE, BX_WARN, memCmp, memCopy, min/max
#include <bx/cpu.h>               // atomicFetchAndAdd, atomicCompareAndSwap
#include <bx/endian.h>            // endianSwap
#include <bx/mutex.h>             // Mutex, MutexScope
#include <bx/pixelformat.h>       // packRg16S, packRgb8
#include <bx/platform.h>          // BX_CACHE_LINE_SIZE
#include <bx/ringbuffer.h>        // RingBufferControl
#include <bx/string.h>            // strCat, strCopy
#include <bx/timer.h>             // getHPCounter, getHPFrequency
#include <bx/uint32_t.h>          // alignUp

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
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wnested-anon-types");
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5039);

#define GLEQ_IMPLEMENTATION
#define GLEQ_STATIC

#include <gleq.h>                 // gleq*

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

#include <TaskScheduler.h>         // ITaskSet, TaskScheduler, TaskSetPartition

#include <mnm_shaders.h>           // *_fs, *_vs


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

constexpr u32 MANAGED_MEMORY_ALIGNMENT = 16;

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


// -----------------------------------------------------------------------------
// ALGEBRAIC TYPES
// -----------------------------------------------------------------------------

using Mat4 = hmm_mat4;

using Vec2 = hmm_vec2;

using Vec3 = hmm_vec3;

using Vec4 = hmm_vec4;

struct Vec2i
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

using Allocator    = bx::AllocatorI;

using CrtAllocator = bx::DefaultAllocator;


// -----------------------------------------------------------------------------
// OWNING ALLOCATOR
// -----------------------------------------------------------------------------

struct OwningAllocator : Allocator
{
    virtual bool owns(const void* ptr) const = 0;
};


// -----------------------------------------------------------------------------
// ARENA ALLOCATOR
// -----------------------------------------------------------------------------

// Simple linear allocator. Enables in-place reallocation of last item and its
// freeing (but only once).
struct ArenaAllocator : OwningAllocator
{
    u8* buffer;
    u32 size;
    u32 top;  // Offset to first free byte in buffer.
    u32 last; // Offset of last allocated block.

    virtual bool owns(const void* ptr) const override
    {
        ASSERT(
            top <= size,
            "Top bigger than the capacity (%" PRIu32 " > %" PRIu32 ").",
            top, size
        );

        return ptr >= buffer && ptr < buffer + top;
    }

    virtual void* realloc(void* ptr, size_t size_, size_t align, const char* file, u32 line) override
    {
        BX_UNUSED(file);
        BX_UNUSED(line);

        if (ptr && !owns(ptr))
        {
            ASSERT(false, "Invalid or not-owned pointer ptr.");
            return nullptr;
        }

        u8* memory = nullptr;

        if (size_)
        {
            u8* data;
            
            if (ptr != buffer + last)
            {
                data = buffer + top;

                if (align)
                {
                    data = reinterpret_cast<u8*>(bx::alignPtr(data, 0, align));
                }
            }
            else
            {
                data = buffer + last;
            }

            if (data + size_ <= buffer + size)
            {
                if (data != ptr)
                {
                    last = top;

                    if (ptr)
                    {
                        // NOTE : We only know the previous allocation's size,
                        //        but since the blocks are allocated linearly,
                        //        the copy will never access data beyond
                        //        `buffer`, even if we copy some waste along.
                        bx::memCopy(data, ptr, size_);
                    }
                }

                memory = data;
                top    = u32((data + size_) - buffer);
            }
        }
        else if (reinterpret_cast<u8*>(ptr) == buffer + last)
        {
            top = last;
        }

        return memory;
    }
};

void init(ArenaAllocator& allocator, void* buffer, u32 size)
{
    ASSERT(buffer, "Invalid buffer pointer.");
    ASSERT(size >= 64, "Too small buffer size %" PRIu32 ".", size);

    allocator.buffer = reinterpret_cast<u8*>(buffer);
    allocator.size   = size;
    allocator.top    = 0;
    allocator.last   = 0;
}

void reset(ArenaAllocator& allocator)
{
    ASSERT(allocator.buffer, "Invalid buffer pointer.");
    ASSERT(allocator.size, "Invalid buffer size.");

    allocator.top  = 0;
    allocator.last = 0;
}


// -----------------------------------------------------------------------------
// STACK ALLOCATOR
// -----------------------------------------------------------------------------

// Simple linear allocator, similar to `ArenaAllocator`, but capable of
// reclaiming freed chunks near the end/top, even if they aren't freed in
// strictly LIFO fashion. Has a bookkeeping overhead of two `u32`s per
// allocation.
struct StackAllocator : OwningAllocator
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
    u32 last;   // Offset of last block header (not necessarily end of previous block data).

    virtual bool owns(const void* ptr) const override
    {
        // NOTE : > (not >=) because the first four bytes are reserved for head.
        return ptr > buffer && ptr < buffer + size;

        ASSERT(
            top <= size,
            "Top bigger than the capacity (%" PRIu32 " > %" PRIu32 ").",
            top, size
        );

        // NOTE : > (not >=) because the first four bytes (plus possibly
        //        necessary alignment) are reserved for head.
        // TODO : We could also check the block is still valid, but it would
        //        require storing a pointer to the first header, or computing it
        //        anew every time.
        return ptr > buffer && ptr < buffer + top;
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

                    // NOTE : There can be a bit of wasted space before the
                    //        block's header and end of previously allocated
                    //        block (to account for proper alignment), that is
                    //        not reclaimed when the block is released. However,
                    //        given how small it generally is, it's probably
                    //        fine as is, without making the logic more complex.
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

        Block block = make_block(data);

        ASSERT(
            bx::isAligned(block.header, i32(std::alignment_of<Header>::value)),
            "New `StackAllocator` block header not aligned to %zu bytes.",
            std::alignment_of<Header>::value
        );
        ASSERT(
            !align || bx::isAligned(block.data, i32(align)),
            "New `StackAllocator` block data not aligned to %zu bytes.",
            align
        );

        return block;
    }
};

void reset(StackAllocator& allocator)
{
    ASSERT(allocator.buffer, "Invalid buffer pointer.");
    ASSERT(allocator.size, "Invalid buffer size.");

    allocator.top    = 0;
    allocator.last   = 0;

    StackAllocator::Block block = allocator.next_block(0);
    block.reset(0, 0);

    allocator.top = block.data - allocator.buffer;
}

void init(StackAllocator& allocator, void* buffer, u32 size)
{
    ASSERT(buffer, "Invalid buffer pointer.");
    ASSERT(size >= 64, "Too small buffer size %" PRIu32 ".", size);
    ASSERT(size <= StackAllocator::SIZE_MASK, "Too big buffer size %" PRIu32".", size);

    allocator.buffer = reinterpret_cast<u8*>(buffer);
    allocator.size   = size;

    reset(allocator);
}


// -----------------------------------------------------------------------------
// BACKED ALLOCATOR
// -----------------------------------------------------------------------------

struct BackedAllocator : Allocator
{
    OwningAllocator* primary = nullptr;
    Allocator*       backing = nullptr;

    virtual void* realloc(void* ptr, size_t size, size_t align, const char* file, u32 line) override
    {
        ASSERT(primary, "Invalid primary allocator pointer.");
        ASSERT(backing, "Invalid backing allocator pointer.");

        void* memory = nullptr;

        if (!size)
        {
            if (primary->owns(ptr))
            {
                memory = primary->realloc(ptr, 0, align, file, line);
            }
            else
            {
                memory = backing->realloc(ptr, 0, align, file, line);
            }
        }
        else if (!ptr)
        {
            memory = primary->realloc(nullptr, size, align, file, line);

            if (!memory)
            {
                memory = backing->realloc(nullptr, size, align, file, line);
            }
        }
        else
        {
            if (primary->owns(ptr))
            {
                memory = primary->realloc(ptr, size, align, file, line);
            }

            if (!memory)
            {
                memory = backing->realloc(ptr, size, align, file, line);
            }
        }

        return memory;
    }
};

void init(BackedAllocator& allocator, OwningAllocator* primary, Allocator* backing)
{
    ASSERT(primary, "Invalid primary allocator pointer.");
    ASSERT(backing, "Invalid backing allocator pointer.");

    allocator.primary = primary;
    allocator.backing = backing;
}


// -----------------------------------------------------------------------------
// DOUBLE FRAME ALLOCATOR (I/II)
// -----------------------------------------------------------------------------

// NOTE : Prototype only since it needs the `DynamicArray` to be defined.
struct DoubleFrameAllocator;


// -----------------------------------------------------------------------------
// ALLOCATION UTILS
// -----------------------------------------------------------------------------

void dealloc_bgfx_memory(void* memory, void* allocator)
{
    BX_FREE(static_cast<Allocator*>(allocator), memory);
}

const bgfx::Memory* alloc_bgfx_memory(Allocator* allocator, u32 size)
{
    return bgfx::makeRef(
        BX_ALLOC(allocator, size),
        size,
        dealloc_bgfx_memory,
        allocator
    );
}


// -----------------------------------------------------------------------------
// SPAN
// -----------------------------------------------------------------------------

template <typename T>
struct Span
{
    T*  data = nullptr;
    u32 size = 0;

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

    T data[Size] = {};

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
        return { const_cast<T*>(data), Size };
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

    T*         data      = nullptr;
    u32        size      = 0;
    u32        capacity  = 0;
    Allocator* allocator = nullptr;

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
        return { const_cast<T*>(data), size };
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


// -----------------------------------------------------------------------------
// DOUBLE FRAME ALLOCATOR (II/II)
// -----------------------------------------------------------------------------

struct DoubleFrameAllocator : Allocator
{
    ArenaAllocator      arenas[2];
    DynamicArray<void*> blocks[2]; // Blocks that didn't fit the arena memory.
    bool                frame;

    virtual void* realloc(void* ptr_, size_t size_, size_t align_, const char* file, u32 line) override
    {
        // NOTE : `ptr_` points to the data itself. Two `u32` values are stored
        //        right before it that contain size of the data (without the
        //         header) and the alignment.

        ArenaAllocator&      arena = arenas[frame];
        DynamicArray<void*>& block = blocks[frame];

        static_assert(
            MANAGED_MEMORY_ALIGNMENT >= sizeof(u32) * 2 &&
            MANAGED_MEMORY_ALIGNMENT >= std::alignment_of<u32>::value,
            "Invalid `DoubleFrameAllocator`'s header assumptions."
        );

        const size_t align = bx::max(align_, size_t(MANAGED_MEMORY_ALIGNMENT));
        const size_t size  = size_ + align;
        void*        ptr   = ptr_ ? reinterpret_cast<u8*>(ptr_) - align : ptr_;

        if (!size_)
        {
            if (arena.owns(ptr))
            {
                arena.realloc(ptr, 0, align, file, line);
            }

            return nullptr;
        }

        void* memory = nullptr;

        if (!ptr || arena.owns(ptr))
        {
            memory = arena.realloc(ptr, size, align, file, line);
        }

        if (!memory)
        {
            // NOTE : `nullptr` memory pointer because we do the copy ourselves.
            memory = block.allocator->realloc(nullptr, size, align, file, line);
        }

        if (memory)
        {
            if (!arena.owns(memory))
            {
                append(block, memory);
            }

            u8*  data   = reinterpret_cast<u8*>(memory) + align;
            u32* header = reinterpret_cast<u32*>(data) - 2;

            WARN(
                bx::isAligned(header, i32(std::alignment_of<u32>::value)),
                "`DoubleFrameAllocator`'s header info not aligned properly."
            );

            if (ptr && memory != ptr)
            {
                bx::memCopy(data, ptr_, *(reinterpret_cast<u32*>(ptr_) - 2));
            }

            header[0] = u32(size_);
            header[1] = u32(align);

            memory = data;
        }

        return memory;
    }
};

void init(DoubleFrameAllocator& allocator, Allocator* backing, void* buffer, u32 size)
{
    void* start = bx::alignPtr(buffer, 0, MANAGED_MEMORY_ALIGNMENT);
    const u32 half_size = (size - u32(uintptr_t(start) - uintptr_t(buffer))) / 2;

    init(allocator.arenas[0], start, half_size);
    init(allocator.arenas[1], reinterpret_cast<u8*>(start) + half_size, half_size);

    for (u32 i = 0; i < 2; i++)
    {
        init   (allocator.blocks[i], backing);
        reserve(allocator.blocks[i], 32);
    }
}

void deinit(DoubleFrameAllocator& allocator)
{
    for (u32 i = 0; i < 2; i++)
    {
        DynamicArray<void*>& blocks = allocator.blocks[i];

        for (u32 j = 0; j < blocks.size; j++)
        {
            BX_ALIGNED_FREE(
                blocks.allocator, 
                blocks[j],
                *(reinterpret_cast<u32*>(blocks[j]) - 1)
            );
        }

        deinit(blocks);
    }
}

void init_frame(DoubleFrameAllocator& allocator)
{
    allocator.frame = !allocator.frame;

    DynamicArray<void*>& blocks = allocator.blocks[allocator.frame];

    for (u32 i = 0; i < blocks.size; i++)
    {
        BX_ALIGNED_FREE(
            blocks.allocator, 
            blocks[i],
            *(reinterpret_cast<u32*>(blocks[i]) - 1)
        );
    }

    reset(allocator.arenas[allocator.frame]);
}


// -----------------------------------------------------------------------------
// FIXED STACK
// -----------------------------------------------------------------------------

template <typename T, u32 Size>
struct FixedStack
{
    T                   top  = {};
    u32                 size = 0;
    FixedArray<T, Size> data;
};

template <typename T, u32 Size>
void init(FixedStack<T, Size>& stack, const T& value = T())
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
void init(MatrixStack<Size>& stack)
{
    init(stack, HMM_Mat4d(1.0f));
}

template <u32 Size>
void multiply_top(MatrixStack<Size>& stack, const Mat4& matrix)
{
    stack.top = matrix * stack.top;
}


// -----------------------------------------------------------------------------
// TIME MEASUREMENT
// -----------------------------------------------------------------------------

struct Timer 
{
    i64 counter   = 0;
    f64 elapsed   = 0;
    f64 frequency = f64(bx::getHPFrequency());
};

void tic(Timer& timer)
{
    timer.counter = bx::getHPCounter();
}

f64 toc(Timer& timer, bool restart = false)
{
    const i64 now = bx::getHPCounter();

    timer.elapsed = (now - timer.counter) / timer.frequency;

    if (restart)
    {
        timer.counter = now;
    }

    return timer.elapsed;
}


// -----------------------------------------------------------------------------
// WINDOW
// -----------------------------------------------------------------------------

struct WindowInfo
{
    Vec2i framebuffer_size      = {};
    Vec2  invariant_size        = {};
    Vec2  position_scale        = {};
    Vec2  display_scale         = {};
    f32   display_aspect        = 0.0f;
    bool  display_scale_changed = false;
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
// WINDOW CURSORS
// -----------------------------------------------------------------------------

struct WindowCursorDesc
{
    u32 cursor;
    int shape;
};

const WindowCursorDesc s_window_cursor_descs[] =
{
    { CURSOR_ARROW    , GLFW_ARROW_CURSOR     },
    { CURSOR_CROSSHAIR, GLFW_CROSSHAIR_CURSOR },
    { CURSOR_H_RESIZE , GLFW_HRESIZE_CURSOR   },
    { CURSOR_HAND     , GLFW_HAND_CURSOR      },
    { CURSOR_I_BEAM   , GLFW_IBEAM_CURSOR     },
    { CURSOR_V_RESIZE , GLFW_VRESIZE_CURSOR   },
};

using WindowCursors = FixedArray<GLFWcursor*, BX_COUNTOF(s_window_cursor_descs)>;

void init(WindowCursors& cursors)
{
    for (u32 i = 0; i < cursors.size; i++)
    {
        ASSERT(s_window_cursor_descs[i].cursor == i,
            "Cursor %i is placed on different index %i.",
            s_window_cursor_descs[i].cursor, i
        );

        cursors[s_window_cursor_descs[i].cursor] = glfwCreateStandardCursor(
            s_window_cursor_descs[i].shape
        );
    }
}

void deinit(WindowCursors& cursors)
{
    for (u32 i = 0; i < cursors.size; i++)
    {
        glfwDestroyCursor(cursors[i]);
    }

    cursors = {};
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

    Vec2                       current  = {};
    Vec2                       previous = {};
    Vec2                       delta    = {};
    Vec2                       scroll   = {};
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
    ValueT value = EnumT::Count;

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
    FixedArray<bgfx::VertexLayout      , 256> layouts;
    FixedArray<bgfx::VertexLayoutHandle, 256> handles;
};

struct VertexLayoutAttribInfo
{
    u32            flag;
    BgfxAttrib     type;
    BgfxAttribType element_type;
    u8             element_count;
    u8             byte_size;
    bool           normalized;
    bool           packed;
};

const VertexLayoutAttribInfo s_vertex_layout_attribs[] =
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
        VERTEX_ATTRIB_MASK  >>  VERTEX_ATTRIB_SHIFT       == 0b00000111 &&
        TEXCOORD_F32        >>  9                         == 0b00001000 &&
        (VERTEX_ATTRIB_MASK >> (VERTEX_ATTRIB_SHIFT - 4)) == 0b01110000 &&
        TEXCOORD_F32        >>  5                         == 0b10000000,
        "Invalid index assumptions in `vertex_layout_index`."
    );

    return
        ((attribs & VERTEX_ATTRIB_MASK) >>  VERTEX_ATTRIB_SHIFT     ) | // Bits 0..2.
        ((attribs & TEXCOORD_F32      ) >>  9                       ) | // Bit  3.
        ((skips   & VERTEX_ATTRIB_MASK) >> (VERTEX_ATTRIB_SHIFT - 4)) | // Bits 4..6.
        ((skips   & TEXCOORD_F32      ) >>  5                       ) ; // Bit  7.
}

constexpr u32 vertex_layout_skips(u32 attribs, u32 alias)
{
    return (attribs & VERTEX_ATTRIB_MASK) & ~(alias & VERTEX_ATTRIB_MASK);
}

void add_vertex_layout(VertexLayoutCache& cache, u32 attribs, u32 skips)
{
    ASSERT(attribs, "Empty attributes.");
    ASSERT((attribs & skips) == 0, "`Attribute and skip flags must be disjoint.");

    bgfx::VertexLayout layout;
    layout.begin();

    for (u32 i = 0; i < BX_COUNTOF(s_vertex_layout_attribs); i++)
    {
        const VertexLayoutAttribInfo& attrib = s_vertex_layout_attribs[i];

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

                ASSERT(
                    (attribs & skips) == skips,
                    "Skips %" PRIu32 "not fully contained in attribs %" PRIu32 ".",
                    skips, attribs
                );

                if (attribs != skips)
                {
                    add_vertex_layout(cache, attribs & ~skips, skips);
                }
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
    u8                data[32]        = {};
    u32               size            = 0;

    PackedColor*      packed_color    = nullptr;
    PackedNormal*     packed_normal   = nullptr;
    PackedTexcoord*   packed_texcoord = nullptr;
    FullTexcoord*     full_texcoord   = nullptr;

    ColorStoreFunc    store_color     = nullptr;
    NormalStoreFunc   store_normal    = nullptr;
    TexcoordStoreFunc store_texcoord  = nullptr;
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


// -----------------------------------------------------------------------------
// GENERIC RECORDING INFO
// -----------------------------------------------------------------------------

enum struct RecordType : u8
{
    NONE,

    FRAMEBUFFER,
    INSTANCES,
    MESH,
};

struct RecordInfo
{
    u32        flags        = 0;
    u32        extra_data   = 0;
    u16        id           = 0;
    bool       is_transform = false;
    RecordType type         = RecordType::NONE;
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
    VertexStoreFunc   store_vertex     = nullptr;
    u32               vertex_count     = 0;
    u32               invocation_count = 0;
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
    Allocator*                temp_allocator,
    void**                    output_remapped_memory = nullptr
)
{
    ASSERT(type == MESH_STATIC || type == MESH_DYNAMIC, "Invalid mesh type.");
    ASSERT(remap_table, "Invalid remapping table.");

    const bgfx::Memory* memory = alloc_bgfx_memory(
        temp_allocator,
        u32(remapped_vertex_count * stream.size)
    );
    ASSERT(memory && memory->data, "Invalid BGFX-created memory.");

    meshopt_remapVertexBuffer(memory->data, stream.data, vertex_count, stream.size, remap_table);

    if (output_remapped_memory)
    {
        *output_remapped_memory = memory->data;
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
    Allocator* temp_allocator,
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
    const bgfx::Memory* memory = alloc_bgfx_memory(
        temp_allocator,
        vertex_count * sizeof(u32)
    );
    ASSERT(memory && memory->data, "Invalid BGFX-created memory.");

    u32* indices = reinterpret_cast<u32*>(memory->data);

    meshopt_remapIndexBuffer(indices, nullptr, vertex_count, remap_table);

    if (optimize && vertex_positions)
    {
        meshopt_optimizeVertexCache(indices, indices, vertex_count,
            indexed_vertex_count
        );

        meshopt_optimizeOverdraw(indices, indices, vertex_count,
            vertex_positions, indexed_vertex_count, 3 * sizeof(f32), 1.05f
        );

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
// NORMALS' GENERATION
// -----------------------------------------------------------------------------

void generate_flat_normals
(
    u32           vertex_count,
    u32           vertex_stride,
    const Vec3*   vertices,
    PackedNormal* normals
)
{
    ASSERT(
        vertex_count % 3 == 0,
        "Vertex count %" PRIu32 " not divisible by 3.",
        vertex_count
    );

    for (u32 i = 0; i < vertex_count; i += 3)
    {
        const Vec3 a = vertices[i + 1] - vertices[i];
        const Vec3 b = vertices[i + 2] - vertices[i];
        const Vec3 n = HMM_Normalize(HMM_Cross(a, b));

        const f32 normalized[] =
        {
            n.X * 0.5f + 0.5f,
            n.Y * 0.5f + 0.5f,
            n.Z * 0.5f + 0.5f,
        };

        // TODO : Accessing `normals` is wrong when attributes contains anything else.

        bx::packRgb8(&normals[i], normalized);

        normals[i + vertex_stride    ] = normals[i];
        normals[i + vertex_stride * 2] = normals[i];
    }
}

float HMM_AngleVec3(Vec3 Left, Vec3 Right)
{
    return acosf(HMM_Clamp(-1.0f, HMM_DotVec3(Left, Right), 1.0f));
}

bool HMM_EpsilonEqualVec3(Vec3 Left, Vec3 Right, float eps = 1e-4f)
{
    const Vec3 diff = Left - Right;

    return
        (HMM_ABS(diff.X) < eps) &
        (HMM_ABS(diff.Y) < eps) &
        (HMM_ABS(diff.Z) < eps);
}

void generate_smooth_normals
(
    u32           vertex_count,
    u32           vertex_stride,
    const Vec3*   vertices,
    Allocator*    temp_allocator,
    PackedNormal* normals
)
{
    ASSERT(
        vertex_count % 3 == 0,
        "Vertex count %" PRIu32 " not divisible by 3.",
        vertex_count
    )

    DynamicArray<u32> unique;
    init(unique, temp_allocator);
    resize(unique, vertex_count, 0u);

    u32 unique_vertex_count = 0;

    for (u32 i = 0; i < vertex_count; i++)
    for (u32 j = 0; j <= i; j++)
    {
        if (HMM_EpsilonEqualVec3(vertices[i], vertices[j]))
        {
            if (i == j)
            {
                unique[i] = unique_vertex_count;
                unique_vertex_count += (i == j);
            }
            else
            {
                unique[i] = unique[j];
            }

            break;
        }
    }

#ifndef NDEBUG
    for (u32 i = 0; i < vertex_count; i++)
    {
        ASSERT(
            unique[i] < unique_vertex_count,
            "Vertex %" PRIu32 " out of the vertex count of %" PRIu32 ".",
            unique[i], unique_vertex_count
        );
    }
#endif // NDEBUG

    union Normal
    {
        Vec3         full;
        PackedNormal packed;
    };

    DynamicArray<Normal> smooth;
    init(smooth, temp_allocator);
    resize(smooth, unique_vertex_count, Normal{HMM_Vec3(0.0f, 0.0f, 0.0f)});

    // https://stackoverflow.com/a/45496726
    for (u32 i = 0; i < vertex_count; i += 3)
    {
        const Vec3 p0 = vertices[i    ];
        const Vec3 p1 = vertices[i + 1];
        const Vec3 p2 = vertices[i + 2];

        const float a0 = HMM_AngleVec3(HMM_NormalizeVec3(p1 - p0), HMM_NormalizeVec3(p2 - p0));
        const float a1 = HMM_AngleVec3(HMM_NormalizeVec3(p2 - p1), HMM_NormalizeVec3(p0 - p1));
        const float a2 = HMM_AngleVec3(HMM_NormalizeVec3(p0 - p2), HMM_NormalizeVec3(p1 - p2));

        const Vec3 n = HMM_Cross(p1 - p0, p2 - p0);

        smooth[unique[i    ]].full += (n * a0);
        smooth[unique[i + 1]].full += (n * a1);
        smooth[unique[i + 2]].full += (n * a2);
    }

    for (u32 i = 0; i < smooth.size; i++)
    {
        if (!HMM_EqualsVec3(smooth[i].full, HMM_Vec3(0.0f, 0.0f, 0.0f)))
        {
            const Vec3 n = HMM_NormalizeVec3(smooth[i].full);

            const f32 normalized[] =
            {
                n.X * 0.5f + 0.5f,
                n.Y * 0.5f + 0.5f,
                n.Z * 0.5f + 0.5f,
            };

            bx::packRgb8(&smooth[i].packed, normalized);
        }
    }

    for (u32 i = 0, j = 0; i < vertex_count; i++, j += vertex_stride)
    {
        normals[j] = smooth[unique[i]].packed;
    }
}


// -----------------------------------------------------------------------------
// MESH & MESH CACHING
// -----------------------------------------------------------------------------

struct Mesh
{
    u32               element_count = 0;
    u32               extra_data    = 0;
    u32               flags         = 0;
    VertexBufferUnion positions     = { bgfx::kInvalidHandle };
    VertexBufferUnion attribs       = { bgfx::kInvalidHandle };
    IndexBufferUnion  indices       = { bgfx::kInvalidHandle };
};

struct MeshCache
{
    Mutex                                                          mutex;
    FixedArray<Mesh, MAX_MESHES>                                   meshes;
    FixedArray<bgfx::TransientVertexBuffer, MAX_TRANSIENT_BUFFERS> transient_buffers;
    u32                                                            transient_buffer_count     = 0;
    u32                                                            transient_memory_exhausted = 0;
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
        ASSERT(
            vertex_count == attribs[i].size / layouts[i]->getStride(),
            "Mismatched number of vertices for attribute buffer %" PRIu32 ".",
            i
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
            remap_table.data, temp_allocator, i ? nullptr : &vertex_positions
        );
    }

    const bool optimize_geometry =
         (flags & OPTIMIZE_GEOMETRY) &&
        ((flags & PRIMITIVE_TYPE_MASK) <= PRIMITIVE_QUADS);

    output_index_buffer = create_persistent_index_buffer(
        type, vertex_count, indexed_vertex_count,
        static_cast<f32*>(vertex_positions), remap_table.data, temp_allocator,
        optimize_geometry
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

    mesh.element_count = recorder.vertex_count;
    mesh.extra_data    = info.extra_data;
    mesh.flags         = info.flags;

    if (type != MESH_TRANSIENT)
    {
        static_assert(
            offsetof(Mesh, positions) + sizeof(Mesh::positions) ==
            offsetof(Mesh, attribs),
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
    bgfx::TextureHandle handle      = BGFX_INVALID_HANDLE;
    u16                 width       = 0;
    u16                 height      = 0;
    BgfxTextureFormat   format      = { bgfx::TextureFormat  ::Count };
    BgfxBackbufferRatio ratio       = { bgfx::BackbufferRatio::Count };
    u32                 read_frame  = U32_MAX;
    bgfx::TextureHandle blit_handle = BGFX_INVALID_HANDLE;
};

struct TextureCache
{
    Mutex                             mutex;
    FixedArray<Texture, MAX_TEXTURES> textures;
};

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

void deinit(TextureCache& cache)
{
    for (u32 i = 0; i < cache.textures.size; i++)
    {
        destroy(cache.textures[i]);
    }
}

void add_texture
(
    TextureCache& cache,
    u16           id,
    u16           flags,
    u16           width,
    u16           height,
    u16           stride,
    const void*   data,
    Allocator*    temp_allocator
)
{
    ASSERT(temp_allocator, "Invalid temporary allocator pointer.");

    constexpr u64 sampling_flags[] =
    {
        BGFX_SAMPLER_NONE,
        BGFX_SAMPLER_POINT,
    };

    constexpr u64 border_flags[] =
    {
        BGFX_SAMPLER_NONE,
        BGFX_SAMPLER_UVW_MIRROR,
        BGFX_SAMPLER_UVW_CLAMP,
    };

    constexpr u64 target_flags[] =
    {
        BGFX_TEXTURE_NONE,
        BGFX_TEXTURE_RT,
    };

    struct FormatInfo
    {
        u32                       size;
        bgfx::TextureFormat::Enum type;
    };

    constexpr FormatInfo formats[] =
    {
        { 4, bgfx::TextureFormat::RGBA8 },
        { 1, bgfx::TextureFormat::R8    },
        { 0, bgfx::TextureFormat::D24S8 },
        { 0, bgfx::TextureFormat::D32F  },
    };

    const FormatInfo format = formats[
        (flags & TEXTURE_FORMAT_MASK) >> TEXTURE_FORMAT_SHIFT
    ];

    bgfx::BackbufferRatio::Enum ratio = bgfx::BackbufferRatio::Count;

    if (width >= SIZE_EQUAL && width <= SIZE_DOUBLE && width == height)
    {
        ratio = bgfx::BackbufferRatio::Enum(width - SIZE_EQUAL);
    }

    const bgfx::Memory* memory = nullptr;

    if (data && format.size > 0 && ratio == bgfx::BackbufferRatio::Count)
    {
        memory = alloc_bgfx_memory(temp_allocator, width * height * format.size);
        ASSERT(memory && memory->data, "Invalid BGFX-created memory.");

        if (stride == 0 || stride == width * format.size)
        {
            bx::memCopy(memory->data, data, memory->size);
        }
        else
        {
            const u8* src = static_cast<const u8*>(data);
            u8*       dst = memory->data;

            for (u16 y = 0; y < height; y++)
            {
                bx::memCopy(dst, src, width * format.size);

                src += stride;
                dst += width * format.size;
            }
        }
    }

    const u64 texture_flags =
        sampling_flags[(flags & TEXTURE_SAMPLING_MASK) >> TEXTURE_SAMPLING_SHIFT] |
        border_flags  [(flags & TEXTURE_BORDER_MASK  ) >> TEXTURE_BORDER_SHIFT  ] |
        target_flags  [(flags & TEXTURE_TARGET_MASK  ) >> TEXTURE_TARGET_SHIFT  ] ;

    Texture texture;

    if (ratio == bgfx::BackbufferRatio::Count)
    {
        texture.handle = bgfx::createTexture2D(
            width, height, false, 1, format.type, texture_flags, memory
        );
    }
    else
    {
        WARN(!memory, "Content of texture %" PRIu16 " ignored.", id);

        texture.handle = bgfx::createTexture2D(
            ratio, false, 1, format.type, texture_flags
        );
    }

    ASSERT(
        bgfx::isValid(texture.handle),
        "Creation of texture %" PRIu16 " failed.", id
    );

    texture.format = format.type;
    texture.ratio  = ratio;
    texture.width  = width;
    texture.height = height;

    {
        MutexScope lock(cache.mutex);

        destroy(cache.textures[id]);

        cache.textures[id] = texture;
    }
}

void remove_texture(TextureCache& cache, u16 id)
{
    MutexScope lock(cache.mutex);

    destroy(cache.textures[id]);
}

void schedule_texture_read
(
    TextureCache&  cache,
    u16            id,
    bgfx::ViewId   pass,
    bgfx::Encoder* encoder,
    void*          output_data
)
{
    MutexScope lock(cache.mutex);

    Texture& texture = cache.textures[id];
    ASSERT(
        bgfx::isValid(texture.handle),
        "Invalid BGFX handle of texture %" PRIu16 " .", id
    );

    if (!bgfx::isValid(texture.blit_handle))
    {
        constexpr u64 flags =
            BGFX_TEXTURE_BLIT_DST  |
            BGFX_TEXTURE_READ_BACK |
            BGFX_SAMPLER_MIN_POINT |
            BGFX_SAMPLER_MAG_POINT |
            BGFX_SAMPLER_MIP_POINT |
            BGFX_SAMPLER_U_CLAMP   |
            BGFX_SAMPLER_V_CLAMP   ;

        texture.blit_handle = texture.ratio == bgfx::BackbufferRatio::Count
            ? texture.blit_handle = bgfx::createTexture2D(
                texture.width, texture.height, false, 1, texture.format, flags
            )
            : texture.blit_handle = bgfx::createTexture2D(
                texture.ratio, false, 1, texture.format, flags
            );

        ASSERT(
            bgfx::isValid(texture.blit_handle),
            "Creating blitting texture failed for texture %" PRIu16 " .", id
        );
    }

    encoder->blit(pass, texture.blit_handle, 0, 0, texture.handle);

    texture.read_frame = bgfx::readTexture(texture.blit_handle, output_data);
}


// -----------------------------------------------------------------------------
// INSTANCE RECORDING
// -----------------------------------------------------------------------------

struct InstanceRecorder
{
    DynamicArray<u8> buffer;
    u16              instance_size = 0;
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
    bgfx::InstanceDataBuffer buffer       = { nullptr, 0, 0, 0, 0, BGFX_INVALID_HANDLE };
    bool                     is_transform = false;
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
// UNIFORMS & UNIFORMS CACHING
// -----------------------------------------------------------------------------

enum struct DefaultUniform : u32
{
    COLOR_TEXTURE_RED,
    COLOR_TEXTURE_RGBA,
    TEXTURE_SIZE,
};

struct DefaultUniformInfo
{
    const char*             name;
    bgfx::UniformType::Enum type;
    DefaultUniform          index;
};

const DefaultUniformInfo s_default_uniforms[] =
{
    { "s_tex_color_r"     , bgfx::UniformType::Sampler, DefaultUniform::COLOR_TEXTURE_RED  },
    { "color_texture_rgba", bgfx::UniformType::Sampler, DefaultUniform::COLOR_TEXTURE_RGBA },
    { "u_tex_size"        , bgfx::UniformType::Vec4   , DefaultUniform::TEXTURE_SIZE       },
};

using DefaultUniforms = FixedArray<bgfx::UniformHandle, BX_COUNTOF(s_default_uniforms)>;

struct UniformCache
{
    Mutex                                         mutex;
    FixedArray<bgfx::UniformHandle, MAX_UNIFORMS> handles;
};

void init(UniformCache& cache)
{
    for (u32 i = 0; i < cache.handles.size; i++)
    {
        cache.handles[i] = BGFX_INVALID_HANDLE;
    }
}

void deinit(UniformCache& cache)
{
    for (u32 i = 0; i < cache.handles.size; i++)
    {
        destroy_if_valid(cache.handles[i]);
    }
}

void add_uniform
(
    UniformCache& cache,
    u16           id,
    u16           type,
    u16           count,
    const char*   name
)
{
    constexpr bgfx::UniformType::Enum types[] =
    {
        bgfx::UniformType::Count,

        bgfx::UniformType::Vec4,
        bgfx::UniformType::Mat4,
        bgfx::UniformType::Mat3,
        bgfx::UniformType::Sampler,
    };

    bgfx::UniformHandle handle = bgfx::createUniform(name, types[type], count);
    ASSERT(bgfx::isValid(handle), "Uniform creation failed.");

    MutexScope lock(cache.mutex);

    destroy_if_valid(cache.handles[id]);
    cache.handles[id] = handle;
}

void init(DefaultUniforms& uniforms)
{
    for (u32 i = 0; i < uniforms.size; i++)
    {
        const u32 index = u32(s_default_uniforms[i].index);

        uniforms[index] = bgfx::createUniform(
            s_default_uniforms[i].name,
            s_default_uniforms[i].type
        );

        ASSERT(bgfx::isValid(uniforms[index]),
            "Failed to create default uniform '%s'.",
            s_default_uniforms[i].name
        );
    }
}

void deinit(DefaultUniforms& uniforms)
{
    for (u32 i = 0; i < uniforms.size; i++)
    {
        destroy_if_valid(uniforms[i]);
    }
}

bgfx::UniformHandle default_sampler
(
    const DefaultUniforms&    uniforms,
    bgfx::TextureFormat::Enum format
)
{
    switch (format)
    {
    case bgfx::TextureFormat::RGBA8:
        return uniforms[u32(DefaultUniform::COLOR_TEXTURE_RGBA)];
    case bgfx::TextureFormat::R8:
        return uniforms[u32(DefaultUniform::COLOR_TEXTURE_RED)];
    default:
        return BGFX_INVALID_HANDLE;
    }
}


// -----------------------------------------------------------------------------
// DEFAULT PROGRAMS
// -----------------------------------------------------------------------------

const bgfx::EmbeddedShader s_default_shaders[] =
{
    BGFX_EMBEDDED_SHADER(position_fs),
    BGFX_EMBEDDED_SHADER(position_vs),

    BGFX_EMBEDDED_SHADER(position_color_fs),
    BGFX_EMBEDDED_SHADER(position_color_vs),

    BGFX_EMBEDDED_SHADER(position_color_normal_fs),
    BGFX_EMBEDDED_SHADER(position_color_normal_vs),

    BGFX_EMBEDDED_SHADER(position_color_texcoord_fs),
    BGFX_EMBEDDED_SHADER(position_color_texcoord_vs),

    BGFX_EMBEDDED_SHADER(position_normal_fs),
    BGFX_EMBEDDED_SHADER(position_normal_vs),

    BGFX_EMBEDDED_SHADER(position_texcoord_fs),
    BGFX_EMBEDDED_SHADER(position_texcoord_vs),

    BGFX_EMBEDDED_SHADER(position_color_r_texcoord_fs),
    BGFX_EMBEDDED_SHADER(position_color_r_pixcoord_fs),

    BGFX_EMBEDDED_SHADER(instancing_position_color_vs),
};

struct DefaultProgramInfo
{
    u32         attribs;
    const char* vs_name;
    const char* fs_name = nullptr;
};

const DefaultProgramInfo s_default_program_info[] =
{
    {
        VERTEX_POSITION, // Position only. It's assumed everywhere else.
        "position"
    },
    {
        VERTEX_COLOR,
        "position_color"
    },
    {
        VERTEX_COLOR | VERTEX_NORMAL,
        "position_color_normal"
    },
    {
        VERTEX_COLOR | VERTEX_TEXCOORD,
        "position_color_texcoord"
    },
    {
        VERTEX_NORMAL,
        "position_normal"
    },
    {
        VERTEX_TEXCOORD,
        "position_texcoord"
    },
    {
        VERTEX_COLOR | INSTANCING_SUPPORTED,
        "instancing_position_color",
        "position_color"
    },
    {
        VERTEX_COLOR | VERTEX_TEXCOORD | SAMPLER_COLOR_R,
        "position_color_texcoord",
        "position_color_r_texcoord"
    },
    {
        VERTEX_COLOR | VERTEX_TEXCOORD | VERTEX_PIXCOORD | SAMPLER_COLOR_R,
        "position_color_texcoord",
        "position_color_r_pixcoord"
    },
};

using DefaultPrograms = FixedArray<bgfx::ProgramHandle, 64>;

constexpr u32 default_program_index(u32 attribs)
{
    static_assert(
        VERTEX_ATTRIB_MASK   >> VERTEX_ATTRIB_SHIFT == 0b000111 &&
        INSTANCING_SUPPORTED >> 17                  == 0b001000 &&
        SAMPLER_COLOR_R      >> 17                  == 0b010000 &&
        VERTEX_PIXCOORD      >> 18                  == 0b100000,
        "Invalid index assumptions in default_program_index`."
    );

    return
        ((attribs & VERTEX_ATTRIB_MASK  ) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
        ((attribs & INSTANCING_SUPPORTED) >> 17                 ) | // Bit 3.
        ((attribs & SAMPLER_COLOR_R     ) >> 17                 ) | // Bit 4.
        ((attribs & VERTEX_PIXCOORD     ) >> 18                 ) ; // Bit 5.
}

void init(DefaultPrograms& programs, bgfx::RendererType::Enum renderer)
{
    fill(programs, BGFX_INVALID_HANDLE);

    char vs_name[32];
    char fs_name[32];

    for (u32 i = 0; i < BX_COUNTOF(s_default_program_info); i++)
    {
        const DefaultProgramInfo& info = s_default_program_info[i];

        bx::strCopy(vs_name, sizeof(vs_name), info.vs_name);
        bx::strCat (vs_name, sizeof(vs_name), "_vs");

        bx::strCopy(fs_name, sizeof(fs_name), info.fs_name ? info.fs_name : info.vs_name);
        bx::strCat (fs_name, sizeof(fs_name), "_fs");

        const bgfx::ShaderHandle vertex = bgfx::createEmbeddedShader(
            s_default_shaders, renderer, vs_name
        );
        ASSERT(
            bgfx::isValid(vertex),
            "Invalid default vertex shader '%s'.",
            vs_name
        );

        const bgfx::ShaderHandle fragment = bgfx::createEmbeddedShader(
            s_default_shaders, renderer, fs_name
        );
        ASSERT(
            bgfx::isValid(fragment),
            "Invalid default fragment shader '%s'.",
            fs_name
        );

        const bgfx::ProgramHandle program = bgfx::createProgram(vertex, fragment, true);
        ASSERT(
            bgfx::isValid(program),
            "Invalid default program with shaders '%s' and '%s'.",
            vs_name, fs_name
        );

        const u32 index = default_program_index(info.attribs);
        programs[index] = program;
    }
}

void deinit(DefaultPrograms& programs)
{
    for (u32 i = 0; i < programs.size; i++)
    {
        destroy_if_valid(programs[i]);
    }
}


// -----------------------------------------------------------------------------
// PROGRAM CACHE
// -----------------------------------------------------------------------------

struct ProgramCache
{
    Mutex                                         mutex;
    FixedArray<bgfx::ProgramHandle, MAX_PROGRAMS> handles;
};

void init(ProgramCache& cache)
{
    fill(cache.handles, BGFX_INVALID_HANDLE);
}

void deinit(ProgramCache& cache)
{
    for (u32 i = 0; i < cache.handles.size; i++)
    {
        destroy_if_valid(cache.handles[i]);
    }
}

void add_program
(
    ProgramCache& cache,
    u16           id,
    const void*   vs_data,
    u32           vs_size,
    const void*   fs_data,
    u32           fs_size
)
{
    ASSERT(
        id < cache.handles.size,
        "Program id %" PRIu16 " out of bounds (%" PRIu32").",
        id, cache.handles.size
    )
    ASSERT(vs_data, "Invalid vertex shader blob pointer.");
    ASSERT(vs_size, "Zero vertex shader blob size.");
    ASSERT(fs_data, "Invalid fragment shader blob pointer.");
    ASSERT(fs_size, "Zero fragment shader blob size.");

    // NOTE : `vs_data` and `fs_data` are assumed to be valid according to
    //        `bgfx::Memory`'s requirements (i.e., at least for two frames).
    bgfx::ShaderHandle  vertex   = bgfx::createShader(bgfx::makeRef(vs_data, vs_size));
    bgfx::ShaderHandle  fragment = bgfx::createShader(bgfx::makeRef(fs_data, fs_size));
    bgfx::ProgramHandle program  = bgfx::createProgram(vertex, fragment, true);

    if (!bgfx::isValid(program))
    {
        ASSERT(false, "Custom program creation failed.");

        destroy_if_valid(vertex);
        destroy_if_valid(fragment);
        destroy_if_valid(program);
    }
    else
    {
        MutexScope lock(cache.mutex);

        destroy_if_valid(cache.handles[id]);

        cache.handles[id] = program;
    }
}


// -----------------------------------------------------------------------------
// FRAMEBUFFER & FRAMEBUFFER RECORDING & FRAMEBUFFER CACHE
// -----------------------------------------------------------------------------

struct FramebufferRecorder
{
    FixedArray<bgfx::TextureHandle, 16> attachments;
    u16                                 count  = 0;
    u16                                 width  = 0;
    u16                                 height = 0;
};

void start(FramebufferRecorder& recorder)
{
    recorder.count  = 0;
    recorder.width  = 0;
    recorder.height = 0;
}

void end(FramebufferRecorder& recorder)
{
    start(recorder);
}

void add_attachment(FramebufferRecorder& recorder, const Texture& attachment)
{
    ASSERT(
        attachment.width > 0 && attachment.height > 0,
        "Zero attachment texture width or height."
    );

    if (!recorder.count)
    {
        recorder.width  = attachment.width;
        recorder.height = attachment.height;
    }

    ASSERT(
        attachment.width == recorder.width && attachment.height == recorder.height,
        "Mismatched framebuffer recording size. Started as %" PRIu16 "x%" PRIu16
        ", but the next attachment texture has size %" PRIu16 "x%" PRIu16".",
        recorder.width, recorder.height, attachment.width, attachment.height
    );

    ASSERT(
        recorder.count < recorder.attachments.size,
        "Maximum attachment texture count (" PRIu32 ") exhausted.",
        recorder.attachments.size
    );

    recorder.attachments[recorder.count++] = attachment.handle;
}

struct Framebuffer
{
    bgfx::FrameBufferHandle handle = BGFX_INVALID_HANDLE;
    u16                     width  = 0;
    u16                     height = 0;
};

struct FramebufferCache
{
    Mutex                                     mutex;
    FixedArray<Framebuffer, MAX_FRAMEBUFFERS> framebuffers;
};

void destroy(Framebuffer& framebuffer)
{
    destroy_if_valid(framebuffer.handle);

    framebuffer = {};
}

void deinit(FramebufferCache& cache)
{
    for (u32 i = 0; i < cache.framebuffers.size; i++)
    {
        destroy(cache.framebuffers[i]);
    }
}

void add_framebuffer
(
    FramebufferCache&                cache,
    u16                              id,
    u16                              width,
    u16                              height,
    const Span<bgfx::TextureHandle>& attachments
)
{
    ASSERT(attachments.size, "Attachment texture list empty.");

    Framebuffer framebuffer;
    framebuffer.width  = width;
    framebuffer.height = height;

    framebuffer.handle = bgfx::createFrameBuffer(
        u8(attachments.size),
        attachments.data,
        false
    );

    ASSERT(bgfx::isValid(framebuffer.handle), "Framebuffer creation failed.");

    {
        MutexScope lock(cache.mutex);

        destroy(cache.framebuffers[id]);

        cache.framebuffers[id] = framebuffer;
    }
}


// -----------------------------------------------------------------------------
// PASS & PASS CACHE
// -----------------------------------------------------------------------------

struct Pass
{
    enum : u8
    {
        DIRTY_NONE        = 0x00,
        DIRTY_CLEAR       = 0x01,
        DIRTY_TOUCH       = 0x02,
        DIRTY_TRANSFORM   = 0x04,
        DIRTY_RECT        = 0x08,
        DIRTY_FRAMEBUFFER = 0x10,
    };

    Mat4                    view_matrix     = HMM_Mat4d(1.0f);
    Mat4                    proj_matrix     = HMM_Mat4d(1.0f);

    u16                     viewport_x      = 0;
    u16                     viewport_y      = 0;
    u16                     viewport_width  = SIZE_EQUAL;
    u16                     viewport_height = SIZE_EQUAL;

    bgfx::FrameBufferHandle framebuffer     = BGFX_INVALID_HANDLE;

    u16                     clear_flags     = BGFX_CLEAR_NONE;
    f32                     clear_depth     = 1.0f;
    u32                     clear_rgba      = 0x000000ff;
    u8                      clear_stencil   = 0;

    u8                      dirty_flags     = DIRTY_CLEAR;
};

struct PassCache
{
    FixedArray<Pass, MAX_PASSES> passes;
    bool                         backbuffer_size_changed = true;
};

void update_passes(PassCache& cache, bgfx::Encoder* encoder)
{
    for (bgfx::ViewId id = 0; id < cache.passes.size; id++)
    {
        Pass& pass = cache.passes[id];

        if (pass.dirty_flags & Pass::DIRTY_TOUCH)
        {
            encoder->touch(id);
        }

        if (pass.dirty_flags & Pass::DIRTY_CLEAR)
        {
            bgfx::setViewClear(
                id, pass.clear_flags, pass.clear_rgba, pass.clear_depth, 
                pass.clear_stencil
            );
        }

        if (pass.dirty_flags & Pass::DIRTY_TRANSFORM)
        {
            bgfx::setViewTransform(id, &pass.view_matrix, &pass.proj_matrix);
        }

        if ((pass.dirty_flags & Pass::DIRTY_RECT) ||
            (cache.backbuffer_size_changed && pass.viewport_width >= SIZE_EQUAL)
        )
        {
            if (pass.viewport_width >= SIZE_EQUAL)
            {
                bgfx::setViewRect(
                    id, pass.viewport_x, pass.viewport_y,
                    bgfx::BackbufferRatio::Enum(pass.viewport_width - SIZE_EQUAL)
                );
            }
            else
            {
                bgfx::setViewRect(
                    id, pass.viewport_x, pass.viewport_y, pass.viewport_width,
                    pass.viewport_height
                );
            }
        }

        if ((pass.dirty_flags & Pass::DIRTY_FRAMEBUFFER) ||
            cache.backbuffer_size_changed
        )
        {
            // Having `BGFX_INVALID_HANDLE` here is OK.
            bgfx::setViewFrameBuffer(id, pass.framebuffer);
        }

        pass.dirty_flags = Pass::DIRTY_NONE;
    }

    cache.backbuffer_size_changed = false;
}


// -----------------------------------------------------------------------------
// DRAW STATE & SUBMISSION
// -----------------------------------------------------------------------------

static_assert(
    BGFX_STATE_DEFAULT == (BGFX_STATE_WRITE_RGB       |
                           BGFX_STATE_WRITE_A         |
                           BGFX_STATE_WRITE_Z         |
                           BGFX_STATE_DEPTH_TEST_LESS |
                           BGFX_STATE_CULL_CW         |
                           BGFX_STATE_MSAA            ) &&

    STATE_DEFAULT      == (STATE_WRITE_RGB            |
                           STATE_WRITE_A              |
                           STATE_WRITE_Z              |
                           STATE_DEPTH_TEST_LESS      |
                           STATE_CULL_CW              |
                           STATE_MSAA                 ) ,

    "BGFX and MiNiMo default draw states don't match."
);

struct DrawState
{
    const InstanceData*      instances       = nullptr;
    u32                      element_start   = 0;
    u32                      element_count   = U32_MAX;
    bgfx::ViewId             pass            = U16_MAX;
    bgfx::FrameBufferHandle  framebuffer     = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      program         = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle      texture         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle      sampler         = BGFX_INVALID_HANDLE;
    u16                      texture_size[2] = {};
    bgfx::VertexLayoutHandle vertex_alias    = BGFX_INVALID_HANDLE;
    u16                      flags           = STATE_DEFAULT;
};

u64 translate_draw_state_flags(u16 flags)
{
    if (flags == STATE_DEFAULT)
    {
        return BGFX_STATE_DEFAULT;
    }

    constexpr u32 BLEND_STATE_MASK       = STATE_BLEND_ADD | STATE_BLEND_ALPHA | STATE_BLEND_MAX | STATE_BLEND_MIN;
    constexpr u32 BLEND_STATE_SHIFT      = 0;

    constexpr u32 CULL_STATE_MASK        = STATE_CULL_CCW | STATE_CULL_CW;
    constexpr u32 CULL_STATE_SHIFT       = 4;

    constexpr u32 DEPTH_TEST_STATE_MASK  = STATE_DEPTH_TEST_GEQUAL | STATE_DEPTH_TEST_GREATER | STATE_DEPTH_TEST_LEQUAL | STATE_DEPTH_TEST_LESS;
    constexpr u32 DEPTH_TEST_STATE_SHIFT = 6;

    constexpr u64 blend_table[] =
    {
        0,
        BGFX_STATE_BLEND_ADD,
        BGFX_STATE_BLEND_ALPHA,
        BGFX_STATE_BLEND_LIGHTEN,
        BGFX_STATE_BLEND_DARKEN,
    };

    constexpr u64 cull_table[] =
    {
        0,
        BGFX_STATE_CULL_CCW,
        BGFX_STATE_CULL_CW,
    };

    constexpr u64 depth_test_table[] =
    {
        0,
        BGFX_STATE_DEPTH_TEST_GEQUAL,
        BGFX_STATE_DEPTH_TEST_GREATER,
        BGFX_STATE_DEPTH_TEST_LEQUAL,
        BGFX_STATE_DEPTH_TEST_LESS,
    };

    // TODO : Remove the conditions from `STATE_MSAA` and onward.
    return
        blend_table     [(flags & BLEND_STATE_MASK     ) >> BLEND_STATE_SHIFT         ] |
        cull_table      [(flags & CULL_STATE_MASK      ) >> CULL_STATE_SHIFT          ] |
        depth_test_table[(flags & DEPTH_TEST_STATE_MASK) >> DEPTH_TEST_STATE_SHIFT    ] |
        (                (flags & STATE_MSAA           ) ?  BGFX_STATE_MSAA        : 0) |
        (                (flags & STATE_WRITE_A        ) ?  BGFX_STATE_WRITE_A     : 0) |
        (                (flags & STATE_WRITE_RGB      ) ?  BGFX_STATE_WRITE_RGB   : 0) |
        (                (flags & STATE_WRITE_Z        ) ?  BGFX_STATE_WRITE_Z     : 0) ;
}

void submit_mesh
(
    const Mesh&                              mesh,
    const Mat4&                              transform,
    const DrawState&                         state,
    const Span<bgfx::TransientVertexBuffer>& transient_buffers,
    const DefaultUniforms&                   default_uniforms,
    bgfx::Encoder&                           encoder
)
{
    constexpr u64 primitive_flags[] =
    {
        0, // Triangles.
        0, // Quads (for users, triangles internally).
        BGFX_STATE_PT_TRISTRIP,
        BGFX_STATE_PT_LINES,
        BGFX_STATE_PT_LINESTRIP,
        BGFX_STATE_PT_POINTS,
    };

    const u16  type        = mesh_type(mesh.flags);
    const bool has_attribs = mesh.flags & VERTEX_ATTRIB_MASK;

    if (type == MESH_STATIC)
    {
                         encoder.setVertexBuffer(0, mesh.positions.static_buffer);
        if (has_attribs) encoder.setVertexBuffer(1, mesh.attribs  .static_buffer, 0, U32_MAX, state.vertex_alias);
                         encoder.setIndexBuffer (   mesh.indices  .static_buffer, state.element_start, state.element_count);
    }
    else if (type == MESH_TRANSIENT)
    {
                         encoder.setVertexBuffer(0, &transient_buffers[mesh.positions.transient_index], state.element_start, state.element_count);
        if (has_attribs) encoder.setVertexBuffer(1, &transient_buffers[mesh.attribs  .transient_index], state.element_start, state.element_count, state.vertex_alias);
    }
    else if (type == MESH_DYNAMIC)
    {
                         encoder.setVertexBuffer(0, mesh.positions.static_buffer);
        if (has_attribs) encoder.setVertexBuffer(1, mesh.attribs  .static_buffer, 0, U32_MAX, state.vertex_alias);
                         encoder.setIndexBuffer (   mesh.indices  .static_buffer, state.element_start, state.element_count);
    }

    if (bgfx::isValid(state.texture) && bgfx::isValid(state.sampler))
    {
        encoder.setTexture(0, state.sampler, state.texture);
    }

    if (mesh.flags & VERTEX_PIXCOORD)
    {
        const f32 data[] =
        {
            f32(state.texture_size[0]),
            f32(state.texture_size[1]),
            f32(state.texture_size[0]) ? 1.0f / f32(state.texture_size[0]) : 0.0f,
            f32(state.texture_size[1]) ? 1.0f / f32(state.texture_size[1]) : 0.0f
        };

        encoder.setUniform(default_uniforms[u32(DefaultUniform::TEXTURE_SIZE)], data);
    }

    encoder.setTransform(&transform);

    u64 flags = translate_draw_state_flags(state.flags);

    flags |= primitive_flags[(mesh.flags & PRIMITIVE_TYPE_MASK) >> PRIMITIVE_TYPE_SHIFT];

    encoder.setState(flags);

    ASSERT(bgfx::isValid(state.program), "Invalid draw state program.");
    encoder.submit(state.pass, state.program);
}


// -----------------------------------------------------------------------------
// TASK MANAGEMENT
// -----------------------------------------------------------------------------

using TaskScheduler = enki::TaskScheduler;

struct TaskPool;

struct Task : enki::ITaskSet
{
    void    (*func)(void*) = nullptr;
    void*     data         = nullptr;
    TaskPool* pool         = nullptr;

    void ExecuteRange(enki::TaskSetPartition, u32) override;
};

struct TaskPool
{
    Mutex                       mutex;
    Task                        tasks[MAX_TASKS]; // TODO : `FixedArray` is limited to trivially copyable types now.
    FixedArray<u8  , MAX_TASKS> nexts;
    u8                          head = 0;

    static_assert(
        MAX_TASKS <= UINT8_MAX,
        "`MAX_TASKS` too big, change the ID type to a bigger type."
    );
};

void init(TaskPool& pool)
{
    for (u8 i = 0; i < MAX_TASKS; i++)
    {
        pool.tasks[i].pool = &pool;
        pool.nexts[i]      = i + 1;
    }
}

Task* acquire_task(TaskPool& pool)
{
    MutexScope lock(pool.mutex);

    Task* task = nullptr;

    if (pool.head < MAX_TASKS)
    {
        const u32 i = pool.head;

        task          = &pool.tasks[i];
        pool.head     =  pool.nexts[i];
        pool.nexts[i] = MAX_TASKS;
    }

    return task;
}

void release_task(TaskPool& pool, const Task* task)
{
    ASSERT(task, "Invalid task pointer.");
    ASSERT(
        task >= &pool.tasks[0] && task <= &pool.tasks[MAX_TASKS - 1],
        "Task not owned by the pool."
    );

    MutexScope lock(pool.mutex);

    const ptrdiff_t i = task - &pool.tasks[0];

    pool.tasks[i].func = nullptr;
    pool.tasks[i].data = nullptr;
    pool.nexts[i]      = pool.head;
    pool.head          = u8(i);
}

void Task::ExecuteRange(enki::TaskSetPartition, u32)
{
    ASSERT(func, "Invalid task function pointer.");
    ASSERT(pool, "Invalid taks pool pointer.");

    (*func)(data);

    release_task(*pool, this);
}


// -----------------------------------------------------------------------------
// MEMORY MANAGEMENT & CACHING
// -----------------------------------------------------------------------------

struct PersistentMemoryCache
{
    Mutex               mutex;
    DynamicArray<void*> blocks;
};

void init(PersistentMemoryCache& cache, Allocator* allocator)
{
    ASSERT(allocator, "Invalid allocator pointer.");

    init(cache.blocks, allocator);
}

void deinit(PersistentMemoryCache& cache)
{
    deinit(cache.blocks);
}

void* alloc(PersistentMemoryCache& cache, u32 size)
{
    if (!size)
    {
        return nullptr;
    }

    void* memory = BX_ALIGNED_ALLOC(
        cache.blocks.allocator,
        size,
        MANAGED_MEMORY_ALIGNMENT
    );
    WARN(memory, "Persistent memory allocation of %" PRIu32 " bytes failed.", size);

    if (memory)
    {
        MutexScope lock(cache.mutex);

        append(cache.blocks, memory);
    }

    return memory;
}

void dealloc(PersistentMemoryCache& cache, void* memory)
{
    if (!memory)
    {
        return;
    }

    MutexScope lock(cache.mutex);

    for (u32 i = 0; i < cache.blocks.size; i++)
    {
        if (cache.blocks[i] == memory)
        {
            BX_ALIGNED_FREE(
                cache.blocks.allocator,
                cache.blocks[i],
                MANAGED_MEMORY_ALIGNMENT
            );

            if (i + 1 < cache.blocks.size)
            {
                cache.blocks[i] = cache.blocks[cache.blocks.size - 1];
            }

            resize(cache.blocks, cache.blocks.size - 1);

            break;
        }
    }
}


// -----------------------------------------------------------------------------
// CODEPOINT QUEUE
// -----------------------------------------------------------------------------

struct CodepointQueue
{
    static constexpr u32 CAPACITY = 32;

    bx::RingBufferControl     buffer = { CAPACITY };
    FixedArray<u32, CAPACITY> codepoints;
};


void flush(CodepointQueue& queue)
{
    queue.buffer.reset();
}

u32 next(CodepointQueue& queue)
{
    if (queue.buffer.available())
    {
        const u32 codepoint = queue.codepoints[queue.buffer.m_read];
        queue.buffer.consume(1);

        return codepoint;
    }

    return 0;
}

void append(CodepointQueue& queue, u32 codepoint)
{
    while (!queue.buffer.reserve(1))
    {
        next(queue);
    }

    queue.codepoints[queue.buffer.m_current] = codepoint;
    queue.buffer.commit(1);
}


// -----------------------------------------------------------------------------
// PLATFORM HELPERS
// -----------------------------------------------------------------------------

} // unnamed namespace

} // namespace rwr

// Compiled separately mainly due to the name clash of `normal` function with
// an enum from MacTypes.h.
extern bgfx::PlatformData create_platform_data
(
    GLFWwindow*              window,
    bgfx::RendererType::Enum renderer
);

namespace rwr
{

namespace
{


// -----------------------------------------------------------------------------
// CONTEXTS
// -----------------------------------------------------------------------------

struct GlobalContext
{
    KeyboardInput     keyboard;
    MouseInput        mouse;

    PassCache         pass_cache;
    MeshCache         mesh_cache;
    InstanceCache     instance_cache;
    TextureCache      texture_cache;
    FramebufferCache  framebuffer_cache;
    UniformCache      uniform_cache;
    ProgramCache      program_cache;
    VertexLayoutCache vertex_layout_cache;
    DefaultUniforms   default_uniforms;
    DefaultPrograms   default_programs;
    PersistentMemoryCache persistent_memory_cache;
    CodepointQueue    codepoint_queue;

    Allocator*        default_allocator = nullptr;

    GLFWwindow*       window_handle     = nullptr;
    WindowInfo        window_info;
    WindowCursors     window_cursors;

    TaskScheduler     task_scheduler;
    TaskPool          task_pool;

    Timer             total_time;
    Timer             frame_time;

    u32               active_cursor     = 0;
    u32               frame_number      = 0;
    u32               bgfx_frame_number = 0;

    u32               transient_memory  = 32_MB; // TODO : Make the name clearer.
    u32               frame_memory      = 8_MB;  // TODO : Make the name clearer.
    u32               vsync_on          = 0;
    bool              reset_back_buffer = true;
};

struct ThreadLocalContext
{
    bgfx::Encoder*       encoder        = nullptr;
    DrawState            draw_state;

    MatrixStack<16>      matrix_stack;

    RecordInfo           record_info;
    MeshRecorder         mesh_recorder;
    InstanceRecorder     instance_recorder;
    FramebufferRecorder  framebuffer_recorder;

    Timer                stop_watch;

    StackAllocator       stack_allocator;
    BackedAllocator      backed_scratch_allocator;
    DoubleFrameAllocator frame_allocator;

    bgfx::ViewId         active_pass    = 0;
    bool                 is_main_thread = false;
};

void init(ThreadLocalContext& ctx, Allocator* allocator, u32 arena_size, u32 stack_size)
{
    void* arena_buffer = BX_ALIGNED_ALLOC(
        allocator,
        arena_size,
        MANAGED_MEMORY_ALIGNMENT
    );
    ASSERT(arena_buffer, "Failed to allocate arena memory.");

    void* stack_buffer = BX_ALIGNED_ALLOC(
        allocator,
        stack_size,
        MANAGED_MEMORY_ALIGNMENT
    );
    ASSERT(stack_buffer, "Failed to allocate stack memory.");

    init(ctx.stack_allocator, stack_buffer, stack_size);

    init(ctx.backed_scratch_allocator, &ctx.stack_allocator, allocator);

    init(ctx.frame_allocator, allocator, arena_buffer, arena_size);

    // NOTE : No `deinit` needed.
    init(ctx.mesh_recorder    , &ctx.stack_allocator);
    init(ctx.instance_recorder, &ctx.stack_allocator);

    init(ctx.matrix_stack);
}

void deinit(ThreadLocalContext& ctx)
{
    Allocator* allocator = ctx.backed_scratch_allocator.backing;

    BX_ALIGNED_FREE(
        allocator,
        ctx.stack_allocator.buffer,
        MANAGED_MEMORY_ALIGNMENT
    );

    BX_ALIGNED_FREE(
        allocator,
        ctx.frame_allocator.arenas[0].buffer,
        MANAGED_MEMORY_ALIGNMENT
    );

    deinit(ctx.frame_allocator);
}

void alloc(ThreadLocalContext*& ctxs, Allocator* allocator, u32 count)
{
    ASSERT(!ctxs, "Valid thread-local context pointer.");
    ASSERT(allocator, "Invalid allocator pointer.");
    ASSERT(count, "Zero thread-local contexts inited.");

    constexpr u32 align = BX_CACHE_LINE_SIZE;
    const     u32 size  = bx::alignUp(sizeof(ThreadLocalContext), align);

    ctxs = reinterpret_cast<ThreadLocalContext*>(BX_ALIGNED_ALLOC(allocator, size * count, align));
    ASSERT(ctxs, "Allocation of %" PRIu16 " thread-local contexts failed.", count);

    for (u32 i = 0; i < count; i++)
    {
        BX_PLACEMENT_NEW(ctxs + i, ThreadLocalContext);
    }
}

void dealloc(ThreadLocalContext*& ctxs, Allocator* allocator, u32 count)
{
    ASSERT(ctxs, "Invalid thread-local context pointer.");
    ASSERT(allocator, "Invalid allocator pointer.");
    ASSERT(count, "Zero thread-local contexts deinited.");

    for (u32 i = 0; i < count; i++)
    {
        ctxs[i].~ThreadLocalContext();
    }

    BX_ALIGNED_FREE(allocator, ctxs, BX_CACHE_LINE_SIZE);

    ctxs = nullptr;
}


// -----------------------------------------------------------------------------
// GLOBAL RUNTIME VARIABLES
// -----------------------------------------------------------------------------

Mutex                            g_mutex;

GlobalContext*                   g_ctx = nullptr;

thread_local ThreadLocalContext* t_ctx = nullptr;


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY (C++)
// -----------------------------------------------------------------------------

int run_impl(void (* init_)(void), void (*setup)(void), void (*draw)(void), void (*cleanup)(void))
{
    MutexScope lock(g_mutex);

    if (init_)
    {
        (*init_)();
    }

    if (glfwInit() != GLFW_TRUE)
    {
        return 1;
    }

    defer(glfwTerminate());

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // Note that this will be ignored when `glfwSetWindowSize` is specified.

    GlobalContext ctx;
    g_ctx = &ctx;

    CrtAllocator crt_allocator;
    g_ctx->default_allocator = &crt_allocator;

    const u32 thread_count = bx::max(3u, std::thread::hardware_concurrency()) - 1u;

    ThreadLocalContext* local_ctxs = nullptr;
    alloc(local_ctxs, g_ctx->default_allocator, thread_count);

    for (u32 i = 0; i < thread_count; i++)
    {
        // TODO : Make scratch size configurable.
        init(local_ctxs[i], g_ctx->default_allocator, g_ctx->frame_memory, 16_MB);
    }

    defer(
        for (u32 i = 0; i < thread_count; i++)
        {
            deinit(local_ctxs[i]);
        }

        dealloc(local_ctxs, g_ctx->default_allocator, thread_count);
    );

    init(g_ctx->task_pool); // NOTE : No `deinit` needed.

    g_ctx->task_scheduler.Initialize(thread_count);
    defer(g_ctx->task_scheduler.WaitforAllAndShutdown());

    ASSERT(
        thread_count == g_ctx->task_scheduler.GetNumTaskThreads(),
        "Mismatched thread-local contexts and task threads count."
    );

    t_ctx = &local_ctxs[0];
    t_ctx->is_main_thread = true;

    struct PinnedTask : enki::IPinnedTask
    {
        ThreadLocalContext* ctx;

        PinnedTask(u32 idx, ThreadLocalContext* ctx)
            : enki::IPinnedTask(idx)
            , ctx(ctx)
        {
        }

        void Execute() override
        {
            ASSERT(
                !t_ctx,
                "Thread-local context for thread %" PRIu32 " already set.",
                threadNum
            );

            t_ctx = ctx;
        }
    };

    for (u32 i = 1; i < g_ctx->task_scheduler.GetNumTaskThreads(); i++)
    {
        PinnedTask task(i, &local_ctxs[i]);

        g_ctx->task_scheduler.AddPinnedTask(&task);
        g_ctx->task_scheduler.WaitforTask(&task);
    }

    g_ctx->window_handle = glfwCreateWindow(
        DEFAULT_WINDOW_WIDTH,
        DEFAULT_WINDOW_HEIGHT,
        "MiNiMo",
        nullptr,
        nullptr
    );

    if (!g_ctx->window_handle)
    {
        return 2;
    }

    defer(glfwDestroyWindow(g_ctx->window_handle));

    update_window_info(g_ctx->window_handle, g_ctx->window_info);

    gleqInit();
    gleqTrackWindow(g_ctx->window_handle);

    {
        // TODO : Set Limits on number of encoders and transient memory.
        // TODO : Init resolution is needed for any backbuffer-size-related
        //        object creations in `setup` function. We should probably just
        //        call the code in the block exectured when
        //        `g_ctx->reset_back_buffer` is true.
        bgfx::Init init;
        init.platformData           = create_platform_data(g_ctx->window_handle, init.type);
        init.resolution.width       = u32(g_ctx->window_info.framebuffer_size.X);
        init.resolution.height      = u32(g_ctx->window_info.framebuffer_size.Y);
        init.limits.transientVbSize = g_ctx->transient_memory;

        if (!bgfx::init(init))
        {
            return 3;
        }
    }

    defer(bgfx::shutdown());

    init(g_ctx->window_cursors);
    defer(deinit(g_ctx->window_cursors));

    // TODO : Setup task scheduler threads and init thread-local-context data:
    //        * memory allocators
    //        * recorders (mesh, ...)

    init(g_ctx->persistent_memory_cache, g_ctx->default_allocator);
    defer(deinit(g_ctx->persistent_memory_cache));

    // NOTE : No `init` needed for these systems.
    defer(deinit(g_ctx->mesh_cache));
    defer(deinit(g_ctx->texture_cache));
    defer(deinit(g_ctx->framebuffer_cache));

    init(g_ctx->vertex_layout_cache);
    defer(deinit(g_ctx->vertex_layout_cache));

    init(g_ctx->default_uniforms);
    defer(deinit(g_ctx->default_uniforms));

    init(g_ctx->uniform_cache);
    defer(deinit(g_ctx->uniform_cache));

    init(g_ctx->default_programs, bgfx::getRendererType());
    defer(deinit(g_ctx->default_programs));

    init(g_ctx->program_cache);
    defer(deinit(g_ctx->program_cache));

    {
        init_frame(g_ctx->mesh_cache);

        for (u32 i = 0; i < thread_count; i++)
        {
            init_frame(local_ctxs[i].frame_allocator);
        }

        if (setup)
        {
            (*setup)();
        }

        g_ctx->bgfx_frame_number = bgfx::frame();
    }

    u32 debug_state = BGFX_DEBUG_NONE;
    bgfx::setDebug(debug_state);

    {
        Pass& pass = g_ctx->pass_cache.passes[0];

        pass.viewport_x      = 0;
        pass.viewport_y      = 0;
        pass.viewport_width  = SIZE_EQUAL;
        pass.viewport_height = SIZE_EQUAL;

        pass.dirty_flags |= Pass::DIRTY_RECT;
    }

    g_ctx->mouse.update_position(
        g_ctx->window_handle,
        HMM_Vec2(
            g_ctx->window_info.position_scale.X,
            g_ctx->window_info.position_scale.Y
        )
    );

    tic(g_ctx->total_time);
    tic(g_ctx->frame_time);

    while (!glfwWindowShouldClose(g_ctx->window_handle))
    {
        g_ctx->keyboard.update_states();
        g_ctx->mouse   .update_states();

        toc(g_ctx->total_time);
        toc(g_ctx->frame_time, true);

        flush(g_ctx->codepoint_queue);

        glfwPollEvents();

        bool   update_cursor_position = false;
        double scroll_accumulator[2]  = { 0.0, 0.0 }; // NOTE : Not sure if we can get multiple scroll events in a single frame.

        GLEQevent event;
        while (gleqNextEvent(&event))
        {
            switch (event.type)
            {
            case GLEQ_KEY_PRESSED:
                g_ctx->keyboard.update_input(event.keyboard.key, InputState::DOWN, f32(g_ctx->total_time.elapsed));
                break;

            case GLEQ_KEY_REPEATED:
                g_ctx->keyboard.update_input(event.keyboard.key, InputState::REPEATED);
                break;

            case GLEQ_KEY_RELEASED:
                g_ctx->keyboard.update_input(event.keyboard.key, InputState::UP);
                break;

            case GLEQ_BUTTON_PRESSED:
                g_ctx->mouse.update_input(event.mouse.button, InputState::DOWN, f32(g_ctx->total_time.elapsed));
                break;

            case GLEQ_BUTTON_RELEASED:
                g_ctx->mouse.update_input(event.mouse.button, InputState::UP);
                break;

            case GLEQ_CURSOR_MOVED:
                update_cursor_position = true;
                break;

            case GLEQ_SCROLLED:
                scroll_accumulator[0] += event.scroll.x;
                scroll_accumulator[1] += event.scroll.y;
                break;

            case GLEQ_CODEPOINT_INPUT:
                append(g_ctx->codepoint_queue, event.codepoint);
                break;

            case GLEQ_FRAMEBUFFER_RESIZED:
            case GLEQ_WINDOW_SCALE_CHANGED:
                g_ctx->reset_back_buffer = true;
                break;

            default:;
                break;
            }

            gleqFreeEvent(&event);
        }

        g_ctx->mouse.scroll[0] = f32(scroll_accumulator[0]);
        g_ctx->mouse.scroll[1] = f32(scroll_accumulator[1]);

        if (g_ctx->reset_back_buffer)
        {
            g_ctx->reset_back_buffer = false;

            update_window_info(g_ctx->window_handle, g_ctx->window_info);

            const u16 width  = u16(g_ctx->window_info.framebuffer_size.X);
            const u16 height = u16(g_ctx->window_info.framebuffer_size.Y);

            const u32 vsync  = g_ctx->vsync_on ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

            bgfx::reset(width, height, BGFX_RESET_NONE | vsync);

            g_ctx->pass_cache.backbuffer_size_changed = true;
        }

        if (update_cursor_position)
        {
            g_ctx->mouse.update_position(
                g_ctx->window_handle,
                HMM_Vec2(
                    g_ctx->window_info.position_scale.X,
                    g_ctx->window_info.position_scale.Y
                )
            );
        }

        g_ctx->mouse.update_position_delta();

        if (key_down(KEY_F12))
        {
            debug_state = debug_state ? BGFX_DEBUG_NONE : BGFX_DEBUG_STATS;
            bgfx::setDebug(debug_state);
        }

        init_frame(g_ctx->mesh_cache);

        for (u32 i = 0; i < thread_count; i++)
        {
            init_frame(local_ctxs[i].frame_allocator);
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (draw)
        {
            (*draw)();
        }

        // TODO : Add some sort of sync mechanism for the tasks that intend to
        //        submit primitives for rendering in a given frame.

        if (t_ctx->is_main_thread)
        {
            if (!t_ctx->encoder)
            {
                t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
                ASSERT(t_ctx->encoder, "Failed to acquire BGFX encoder.");
            }

            // TODO : ??? Touch all active passes in all local contexts ???
            g_ctx->pass_cache.passes[t_ctx->active_pass].dirty_flags |= Pass::DIRTY_TOUCH;

            update_passes(g_ctx->pass_cache, t_ctx->encoder);
        }

        for (u32 i = 0; i < thread_count; i++)
        {
            if (local_ctxs[i].encoder)
            {
                bgfx::end(local_ctxs[i].encoder);

                local_ctxs[i].encoder = nullptr;
            }
        }

        g_ctx->bgfx_frame_number = bgfx::frame();
        bx::atomicFetchAndAdd(&g_ctx->frame_number, 1u);
    }

    if (cleanup)
    {
        (*cleanup)();
    }

    // ...

    return 0;
}


// -----------------------------------------------------------------------------

} // unnamed namespace

} // namespace rwr


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY FROM C++
// -----------------------------------------------------------------------------

int run(void (* init)(void), void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    return rwr::run_impl(init, setup, draw, cleanup);
}


// -----------------------------------------------------------------------------

} // namespace mnm


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MAIN ENTRY FROM C
// -----------------------------------------------------------------------------

int mnm_run(void (* init)(void), void (* setup)(void), void (* draw)(void), void (* cleanup)(void))
{
    return mnm::rwr::run_impl(init, setup, draw, cleanup);
}
