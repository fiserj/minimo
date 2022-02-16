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

template <u32 Capacity>
struct MatrixStack : StaticStack<Mat4, Capacity>
{
    using Parent = StaticStack<Mat4, Capacity>;

    MatrixStack()
    {
        reset();
    }

    inline void reset()
    {
        Parent::reset(HMM_Mat4d(1.0f));
    }

    inline void multiply_top(const Mat4& matrix)
    {
        Parent::top = matrix * Parent::top;
    }
};

} // namespace mnm

