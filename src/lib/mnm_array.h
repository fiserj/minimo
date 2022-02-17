#pragma once

namespace mnm
{

internal constexpr u8 s_zero_memory[32] = {};

internal void fill_pattern(void* dst, const void* pattern, u32 size, u32 count)
{
    ASSERT(dst);
    ASSERT(pattern);
    ASSERT(size);
    ASSERT(count);

    if (size <= sizeof(s_zero_memory) && 0 == bx::memCmp(pattern, s_zero_memory, size))
    {
        bx::memSet(dst, 0, size * count);
    }
    else
    {
        for (u32 i = 0; i < count; i++)
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

template <typename T, u32 Size>
struct StaticArray
{
    T data[Size];

    static_assert(
        is_pod<T>(),
        "`StaticArray` only supports POD-like types."
    );

    static_assert(
        Size > 0,
        "`StaticArray` must have positive size."
    );

    inline const T& operator[](u32 i) const
    {
        ASSERT(i < Size);
        return data[i];
    }

    inline T& operator[](u32 i)
    {
        ASSERT(i < Size);
        return data[i];
    }

    void fill(const T& value)
    {
        fill_value(data, value, Size);
    }
};

template <typename T>
struct DynamicArray
{
    T*              data;
    u32             size;
    u32             capacity;
    bx::AllocatorI* allocator;

    static_assert(
        is_pod<T>(),
        "`DynamicArray` only supports POD-like types."
    );

    // TODO vvvv : Retire these in favor of a delayed init method ?-------------
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
    // TODO ^^^^ : Retire these in favor of a delayed init method ?-------------

    inline const T& operator[](u32 i) const
    {
        ASSERT(data && i < size);
        return data[i];
    }

    inline T& operator[](u32 i)
    {
        ASSERT(data && i < size);
        return data[i];
    }

    inline void clear()
    {
        size = 0;
    }

    inline bool is_empty() const
    {
        return size == 0;
    }

    inline void reserve(u32 new_capacity)
    {
        if (new_capacity > capacity)
        {
            capacity = new_capacity;
            data     = static_cast<T*>(BX_REALLOC(allocator, data, capacity * sizeof(T)));

            ASSERT(data);
        }
    }

    inline void resize(u32 new_size)
    {
        if (new_size > capacity)
        {
            reserve(capacity_hint(new_size));
        }

        size = new_size;
    }

    void resize(u32 new_size, const T& value)
    {
        const u32 old_size = size;

        resize(new_size);

        if (new_size > old_size)
        {
            fill_value(data + old_size, value, new_size - old_size);
        }
    }

    void push(const T& value)
    {
        ASSERT(&value < data || &value >= data + capacity);

        if (size == capacity)
        {
            reserve(capacity_hint(size + 1));
        }

        bx::memCopy(data + size, &value, sizeof(value));

        size++;
    }

    void pop()
    {
        ASSERT(size > 0);
        size--;
    }

    inline const T& front() const
    {
        return operator[](0);
    }

    inline T& front()
    {
        return operator[](0);
    }

    inline const T& back() const
    {
        return operator[](size - 1);
    }

    inline T& back()
    {
        return operator[](size - 1);
    }

private:
    inline u32 capacity_hint(u32 requested_size) const
    {
        return bx::max(u32(8), requested_size, capacity + capacity / 2);
    }
};

template <typename T>
inline DynamicArray<T> create_dynamic_array(Allocator* allocator)
{
    ASSERT(allocator);

    DynamicArray<T> array = {};
    array.allocator = allocator;

    return array;
}

template <typename T>
inline void destroy(DynamicArray<T>& array)
{
    ASSERT(array.allocator);

    BX_FREE(array.allocator, array.data);

    array = {};
}

template <u32 Size>
inline void push_back(DynamicArray<u8>& buffer, const void* data)
{
    static_assert(Size > 0, "Size must be positive.");

    buffer.resize(buffer.size + Size);

    assign<Size>(data, buffer.data + buffer.size - Size);
}

internal inline void push_back(DynamicArray<u8>& buffer, const void* data, u32 size)
{
    buffer.resize(buffer.size + size);

    bx::memCopy(buffer.data + buffer.size - size, data, size);
}

template <typename T>
inline void push_back(DynamicArray<u8>& buffer, const T& value)
{
    push_back<sizeof(T)>(buffer, &value);
}

} // namespace mnm
