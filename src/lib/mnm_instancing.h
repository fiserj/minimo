#pragma once

namespace mnm
{

struct InstanceRecorder
{
    DynamicArray<u8> buffer;
    u16              instance_size = 0;

    void reset(u32 type)
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

        buffer.clear();
        instance_size = type_sizes[bx::max<u32>(type, BX_COUNTOF(type_sizes) - 1)];
    }

    inline void clear()
    {
        buffer.clear();
        instance_size = 0;
    }

    inline void instance(const void* data)
    {
        ASSERT(data);

        push_back(buffer, data, instance_size);
    }

    inline u32 instance_count() const
    {
        return buffer.size / instance_size;
    }
};

} // namespace mnm
