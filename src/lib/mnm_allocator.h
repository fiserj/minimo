#pragma once

namespace mnm
{

using CrtAllocator = bx::DefaultAllocator;

struct OwningAllocator : Allocator
{
    virtual void* realloc(void*, size_t, size_t, const char*, u32) = 0;

    virtual bool owns(const void*) const = 0;
};

struct StackAllocator : OwningAllocator
{
    u8* buffer;
    u32 capacity;
    u32 head;

    void init(void* buffer_, u32 size)
    {
        ASSERT(buffer_ && bx::isAligned(uintptr_t(buffer_), sizeof(head)));
        ASSERT(size >= 64);

        buffer   = reinterpret_cast<u8*>(buffer_);
        capacity = size;
        head     = sizeof(head);

        *reinterpret_cast<u32*>(buffer) = 0;
    }

    virtual bool owns(const void* ptr) const override
    {
        // NOTE : > (not >=) because the first four bytes are reserved for head.
        return ptr > buffer && ptr < buffer + capacity;
    }

    virtual void* realloc(void* ptr, size_t size, size_t align, const char* file, u32 line) override
    {
        BX_UNUSED(file);
        BX_UNUSED(line);

        ASSERT(!ptr || owns(ptr));

        u8* memory = nullptr;

        if (!size)
        {
            if (ptr)
            {
                const u32   prev_head = *(reinterpret_cast<u32*>(ptr) - 1);
                const void* prev_ptr  = bx::alignPtr(buffer + prev_head,
                    sizeof(head), bx::max(align, sizeof(head)));

                if (ptr == prev_ptr)
                {
                    head = prev_head;
                }
            }
        }
        else if (!ptr)
        {
            if (size)
            {
                memory = reinterpret_cast<u8*>(bx::alignPtr(buffer + head,
                    sizeof(head), bx::max(align, sizeof(head))));

                const u8* memory_end = memory + size;

                if (memory_end <= buffer + capacity)
                {
                    *(reinterpret_cast<u32*>(memory) - 1) = head;
                    head = u32(memory_end - buffer);
                }
                else
                {
                    memory = nullptr;
                }
            }
        }
        else
        {
            const u32 prev_head = *(reinterpret_cast<u32*>(ptr) - 1);
            u8*       prev_ptr  =   reinterpret_cast<u8*>(bx::alignPtr(
                buffer + prev_head, sizeof(head), bx::max(align, sizeof(head))));

            if (ptr == prev_ptr)
            {
                // TODO : Multiples of alignment wouldn't matter either.
                ASSERT(!align || bx::isAligned(uintptr_t(prev_ptr), align));

                const u8* memory_end = prev_ptr + size;

                if (memory_end <= buffer + capacity)
                {
                    memory = prev_ptr;
                    head   = u32(memory_end - buffer);
                }
            }
            else
            {
                memory = reinterpret_cast<u8*>(realloc(nullptr, size, align, file, line));

                if (memory)
                {
                    bx::memCopy(memory, ptr, size);
                }
            }
        }

        return memory;
    }
};

} // namespace mnm
