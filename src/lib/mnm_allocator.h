#pragma once

namespace mnm
{

using CrtAllocator = bx::DefaultAllocator;

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

        inline u32 size() const
        {
            return header->flags & SIZE_MASK;
        }

        inline bool is_valid() const
        {
            return header->flags & VALID_BIT;
        }

        inline void invalidate()
        {
            header->flags &= ~VALID_BIT;
        }

        inline void reset(u32 prev_, u32 size_)
        {
            header->prev  = prev_;
            header->flags = u32(size_) | VALID_BIT;
        }
    };

    u8* buffer;   // First 8 bytes reserved for a sentinel block.
    u32 capacity; // Total buffer size in bytes.
    u32 top;      // Offset to first free byte in buffer.
    u32 last;     // Offset of last block header.

    void init(void* buffer_, u32 size)
    {
        ASSERT(buffer_);
        ASSERT(size >= 64 && size <= SIZE_MASK);

        buffer   = reinterpret_cast<u8*>(buffer_);
        capacity = size;
        top      = 0;
        last     = 0;

        Block block = next_block(0);
        block.reset(0, 0);

        top = block.data - buffer;
    }

    inline bool owns(const void* ptr) const
    {
        // NOTE : > (not >=) because the first four bytes are reserved for head.
        return ptr > buffer && ptr < buffer + capacity;
    }

    virtual void* realloc(void* ptr, size_t size, size_t align, const char* file, u32 line) override
    {
        BX_UNUSED(file);
        BX_UNUSED(line);

        ASSERT(!ptr || owns(ptr));
        ASSERT(size <= SIZE_MASK);

        u8* memory = nullptr;

        if (!size)
        {
            if (ptr)
            {
                Block block = make_block(ptr);
                ASSERT(block.is_valid());

                if (block.header == make_block(last).header)
                {
                    for (;;)
                    {
                        block = make_block(block.header->prev);
                        last  = block.data - buffer - sizeof(Header);
                        top   = block.data - buffer + block.size();

                        ASSERT(top >= sizeof(Header));

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
            if (size)
            {
                Block block = next_block(align);

                if (block.data + size <= buffer + capacity)
                {
                    block.reset(last, size);

                    last   = block.data - buffer - sizeof(Header);
                    top    = block.data - buffer + size;
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
                ASSERT(!align || bx::isAligned(uintptr_t(block.data), align));
                ASSERT(block.is_valid());

                if (block.data + size <= buffer + capacity)
                {
                    block.reset(block.header->prev, size);

                    top    = block.data - buffer + size;
                    memory = block.data;
                }
            }
            else
            {
                memory = reinterpret_cast<u8*>(realloc(nullptr, size, align, file, line));

                if (memory)
                {
                    bx::memCopy(memory, ptr, size);

                    block.invalidate();
                }
            }
        }

        return memory;
    }

    inline Block make_block(void* data_ptr)
    {
        Block block;
        block.header = reinterpret_cast<Header*>(data_ptr) - 1;
        block.data   = reinterpret_cast<u8*>(data_ptr);

        return block;
    }

    inline Block make_block(u32 header_offset)
    {
        return make_block(buffer + header_offset + sizeof(Header));
    }

    inline Block next_block(size_t align)
    {
        void* data = bx::alignPtr(buffer + top, sizeof(Header), bx::max(align, std::alignment_of<Header>::value));

        return make_block(data);
    }
};

struct BackedStackAllocator : StackAllocator
{
    CrtAllocator allocator;

    virtual void* realloc(void* ptr, size_t size, size_t align, const char* file, u32 line) override
    {
        void* memory;

        if (!size)
        {
            if (owns(ptr))
            {
                memory = StackAllocator::realloc(ptr, 0, align, file, line);
            }
            else
            {
                memory = allocator.realloc(ptr, 0, align, file, line);
            }
        }
        else if (!ptr)
        {
            memory = StackAllocator::realloc(nullptr, size, align, file, line);

            if (!memory)
            {
                memory = allocator.realloc(nullptr, size, align, file, line);
            }
        }
        else
        {
            if (owns(ptr))
            {
                memory = StackAllocator::realloc(ptr, size, align, file, line);
            }

            if (!memory)
            {
                memory = allocator.realloc(ptr, size, align, file, line);
            }
        }

        return memory;
    }
};

} // namespace mnm
