#pragma once

namespace mnm
{

struct InstanceRecorder
{
    void begin(u16 id_, u16 type)
    {
        ASSERT(!is_recording() || (id_ == UINT16_MAX && type == UINT16_MAX));

        constexpr u16 type_sizes[] =
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

        id            = id_;
        instance_size = type_sizes[std::max<size_t>(type, BX_COUNTOF(type_sizes) - 1)];
        is_transform  = type == INSTANCE_TRANSFORM;

        buffer.clear();
    }

    inline void end()
    {
        ASSERT(is_recording());

        begin(UINT16_MAX, UINT16_MAX);
    }

    inline void instance(const void* data)
    {
        ASSERT(data);
        ASSERT(is_recording());

        push_back(buffer, data, instance_size);
    }

    inline u32 instance_count() const
    {
        return u32(buffer.size / instance_size);
    }

    inline bool is_recording() const
    {
        return id != UINT16_MAX;
    }

    DynamicArray<u8> buffer;
    u16              id            = UINT16_MAX;
    u16              instance_size = 0;
    bool             is_transform  = false;
};

} // namespace mnm
