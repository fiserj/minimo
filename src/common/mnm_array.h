#pragma once

#include <stdint.h>       // uint*_t

#include <bx/allocator.h> // AllocatorI
#include <bx/bx.h>        // max, mem*

#include <type_traits>    // is_trivial, is_standard_layout

#ifndef ASSERT
#   include <assert.h>    // assert
#   define ASSERT(cond) assert(cond)
#endif

namespace mnm
{

struct ArrayBase
{
    static bx::AllocatorI* default_allocator()
    {
        static bx::DefaultAllocator s_allocator;

        return &s_allocator;
    }
};

// Simplified `std::vector`-like class. Supports only POD-like types.
template <typename T>
struct Array : ArrayBase
{
    T*              data      = nullptr;
    uint32_t        size      = 0;
    uint32_t        capacity  = 0;
    bx::AllocatorI* allocator = nullptr;

    static_assert(
        std::is_trivial<T>::value && std::is_standard_layout<T>::value,
        "`Array<T>` only supports POD-like types."
    );

    Array(bx::AllocatorI* allocator = default_allocator())
        : allocator(allocator)
    {
    }

    Array(const Array<T>& other)
        : allocator(other.allocator)
    {
        operator=(other);
    }

    Array<T>& operator=(const Array<T>& other)
    {
        clear();

        resize(other.size);

        bx::memCopy(data, other.data, other.size * sizeof(T));

        return *this;
    }

    ~Array()
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

    void reserve(uint32_t new_capacity)
    {
        if (new_capacity > capacity)
        {
            capacity = new_capacity;
            data     = static_cast<T*>(BX_REALLOC(allocator, data, capacity * sizeof(T)));

            ASSERT(data);
        }
    }

    uint32_t next_capacity(uint32_t requested_size) const
    {
        return bx::max(UINT32_C(8), requested_size, capacity + capacity / 2);
    }

    void resize(uint32_t new_size)
    {
        if (new_size > capacity)
        {
            reserve(next_capacity(new_size));
        }

        size = new_size;
    }

    void resize(uint32_t new_size, const T& value)
    {
        if (new_size > capacity)
        {
            reserve(next_capacity(new_size));
        }

        if (new_size > size)
        {
            static constexpr uint8_t s_zero_memory[sizeof(T)] = {};

            if (0 == bx::memCmp(&value, s_zero_memory, sizeof(T)))
            {
                bx::memSet(data + size, 0, (new_size - size) * sizeof(T));
            }
            else
            {
                for (uint32_t i = size; i < new_size; i++)
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

    const T& operator[](uint32_t i) const
    {
        ASSERT(data && i < size);
        return data[i];
    }

    T& operator[](uint32_t i)
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

} // namespace mnm
