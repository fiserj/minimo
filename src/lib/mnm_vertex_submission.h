#pragma once

namespace mnm
{

using VertexStoreFunc = void (*)
(
    const Vec3&,
    const VertexAttribState&,
    DynamicArray<u8>&,
    DynamicArray<u8>&,
    u32&,
    u32&
);

template <u32 Size>
inline void emulate_quad(DynamicArray<u8>& buffer)
{
    static_assert(Size > 0, "Size must be positive.");

    ASSERT(!buffer.is_empty());
    ASSERT( buffer.size % Size      == 0);
    ASSERT((buffer.size / Size) % 3 == 0);

    buffer.resize(buffer.size + 2 * Size);

    u8* end = buffer.data + buffer.size;

    // Assuming the last triangle has relative indices
    // [v0, v1, v2] = [-5, -4, -3], we need to copy the vertices v0 and v2.
    assign<Size>(end - 5 * Size, end - 2 * Size);
    assign<Size>(end - 3 * Size, end - 1 * Size);
}

template <u16 Flags>
void store_vertex
(
    const Vec3&              position,
    const VertexAttribState& attrib_state,
    DynamicArray<u8>&        attrib_buffer,
    DynamicArray<u8>&        position_buffer,
    u32&                     vertex_count,
    u32&                     invocation_count
)
{
    if constexpr (!!(Flags & (PRIMITIVE_QUADS)))
    {
        if ((invocation_count & 3) == 3)
        {
            emulate_quad<sizeof(position)>(position_buffer);

            if constexpr (!!(Flags & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)))
            {
                emulate_quad<vertex_attribs_size<Flags>()>(attrib_buffer);
            }

            vertex_count += 2;
        }

        invocation_count++;
    }

    vertex_count++;

    push_back(position_buffer, position);

    if constexpr (!!(Flags & (VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD)))
    {
        push_back<vertex_attribs_size<Flags>()>(attrib_buffer, attrib_state.data);
    }
}

class VertexStoreFuncTable
{
    StaticArray<VertexStoreFunc, 32> m_table;

public:
    void init()
    {
        //      +---------- VERTEX_COLOR
        //      |  +------- VERTEX_NORMAL
        //      |  |  +---- VERTEX_TEXCOORD
        //      |  |  |
        variant<0, 0, 0>();
        variant<1, 0, 0>();
        variant<0, 1, 0>();
        variant<0, 0, 1>();
        variant<1, 1, 0>();
        variant<1, 0, 1>();
        variant<0, 1, 1>();
        variant<1, 1, 1>();
    }

    inline const VertexStoreFunc& operator[](u16 flags) const
    {
        return m_table[index(flags)];
    }

private:
    static inline constexpr u16 index(u16 flags)
    {
        static_assert(
            VERTEX_ATTRIB_MASK >> VERTEX_ATTRIB_SHIFT == 0b00111 &&
            TEXCOORD_F32       >> 9                   == 0b01000 &&
            PRIMITIVE_QUADS                           == 0b10000,
            "Invalid index assumptions in `VertexPushFuncTable::index`."
        );

        return
            ((flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
            ((flags & TEXCOORD_F32      ) >> 9                  ) | // Bit 3.
            ((flags & PRIMITIVE_QUADS   )                       ) ; // Bit 4.
    }

    template <
        bool HasColor,
        bool HasNormal,
        bool HasTexCoord,
        bool HasTexCoordF32    = false,
        bool HasPrimitiveQuads = false
    >
    void variant()
    {
        if constexpr (HasTexCoord && !HasTexCoordF32)
        {
            variant<HasColor, HasNormal, HasTexCoord, true, HasPrimitiveQuads>();
        }

        if constexpr (!HasPrimitiveQuads)
        {
            variant<HasColor, HasNormal, HasTexCoord, HasTexCoordF32, true>();
        }

        constexpr u16 Flags =
            (HasColor          ? VERTEX_COLOR    : 0) |
            (HasNormal         ? VERTEX_NORMAL   : 0) |
            (HasTexCoord       ? VERTEX_TEXCOORD : 0) |
            (HasTexCoordF32    ? TEXCOORD_F32    : 0) |
            (HasPrimitiveQuads ? PRIMITIVE_QUADS : 0) ;

        // NOTE : We do insert few elements multiple times.
        m_table[index(Flags)] = store_vertex<Flags>;
    }
};

} // namespace mnm
