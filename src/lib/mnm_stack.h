#pragma once

namespace mnm
{

template <typename T, u32 Capacity>
struct StaticStack
{
    T                        top;
    u32                      size;
    StaticArray<T, Capacity> data;

    inline void reset(const T& value = T())
    {
        top  = value;
        size = 0;
    }

    inline void push()
    {
        data[size++] = top;
    }

    inline void pop()
    {
        top = data[--size];
    }
};

template <u32 Capacity = 16>
struct MatrixStack : StaticStack<Mat4, Capacity>
{
    inline void reset()
    {
        reset(HMM_Mat4d(1.0f));
    }

    inline void multiply_top(const Mat4& matrix)
    {
        top = matrix * top;
    }
};

} // namespace mnm

