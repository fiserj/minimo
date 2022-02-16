#pragma once

#include <assert.h>       // assert
#include <stdint.h>       // *int*_t

#include <type_traits>    // is_trivial, is_standard_layout

#include <bx/allocator.h> // AllocatorI
#include <bx/bx.h>        // max, mem*

namespace mnm
{

// -----------------------------------------------------------------------------
// DYNAMIC ARRAY
// -----------------------------------------------------------------------------

struct DynamicArrayBase
{
    static bx::AllocatorI* default_allocator()
    {
        static bx::DefaultAllocator s_allocator;

        return &s_allocator;
    }
};

// Simplified `std::vector`-like class. Supports only POD-like types.
template <typename T>
struct DynamicArray : DynamicArrayBase
{
    T*              data      = nullptr;
    u32             size      = 0;
    u32             capacity  = 0;
    bx::AllocatorI* allocator = nullptr;

    static_assert(
        std::is_trivial<T>::value && std::is_standard_layout<T>::value,
        "`DynamicArray<T>` only supports POD-like types."
    );

    DynamicArray(bx::AllocatorI* allocator = default_allocator())
        : allocator(allocator)
    {
    }

    DynamicArray(const DynamicArray<T>& other)
        : allocator(other.allocator)
    {
        operator=(other);
    }

    DynamicArray<T>& operator=(const DynamicArray<T>& other)
    {
        clear();

        resize(other.size);

        bx::memCopy(data, other.data, other.size * sizeof(T));

        return *this;
    }

    ~DynamicArray()
    {
        BX_FREE(allocator, data);
    }

    bool is_empty() const
    {
        return size == 0;
    }

    void clear()
    {
        size = 0;
    }

    void reserve(u32 new_capacity)
    {
        if (new_capacity > capacity)
        {
            capacity = new_capacity;
            data     = static_cast<T*>(BX_REALLOC(allocator, data, capacity * sizeof(T)));

            ASSERT(data);
        }
    }

    u32 next_capacity(u32 requested_size) const
    {
        return bx::max(u32(8), requested_size, capacity + capacity / 2);
    }

    void resize(u32 new_size)
    {
        if (new_size > capacity)
        {
            reserve(next_capacity(new_size));
        }

        size = new_size;
    }

    void resize(u32 new_size, const T& value)
    {
        if (new_size > capacity)
        {
            reserve(next_capacity(new_size));
        }

        if (new_size > size)
        {
            static constexpr u8 s_zero_memory[sizeof(T)] = {};

            if (0 == bx::memCmp(&value, s_zero_memory, sizeof(T)))
            {
                bx::memSet(data + size, 0, (new_size - size) * sizeof(T));
            }
            else
            {
                for (u32 i = size; i < new_size; i++)
                {
                    bx::memCopy(data + i, &value, sizeof(value));
                }
            }
        }

        size = new_size;
    }

    void push_back(const T& value)
    {
        ASSERT(&value < data || &value >= data + capacity);

        if (size == capacity)
        {
            reserve(next_capacity(size + 1));
        }

        bx::memCopy(data + size, &value, sizeof(value));

        size++;
    }

    void pop_back()
    {
        ASSERT(size > 0);
        size--;
    }

    const T& operator[](u32 i) const
    {
        ASSERT(data && i < size);
        return data[i];
    }

    T& operator[](u32 i)
    {
        ASSERT(data && i < size);
        return data[i];
    }

    const T& front() const
    {
        return operator[](0);
    }

    T& front()
    {
        return operator[](0);
    }

    const T& back() const
    {
        return operator[](size - 1);
    }

    T& back()
    {
        return operator[](size - 1);
    }
};


// -----------------------------------------------------------------------------

} // namespace mnm
