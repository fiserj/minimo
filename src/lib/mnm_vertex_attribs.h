#pragma once

namespace mnm
{

template <u16 Flags>
constexpr u32 vertex_attribs_size();

template <u16 Flags, u16 Attrib>
constexpr u32 vertex_attrib_offset();

BX_ALIGN_DECL_16(struct) VertexAttribState
{
    u8 data[32];

    using PackedColorType    = u32; // As RGBA_u8.

    using PackedNormalType   = u32; // As RGB_u8.

    using PackedTexcoordType = u32; // As RG_s16.

    using FullTexcoordType   = Vec2;

    template <typename T, u32 BytesOffset>
    inline constexpr const T* at() const
    {
        static_assert(is_pod<T>(),
            "`T` must be POD type.");

        static_assert(BytesOffset % std::alignment_of<T>::value == 0,
            "`BytesOffset` must be multiple of alignment of return type `T`.");

        return reinterpret_cast<const T*>(data + BytesOffset);
    }

    template <typename T, u32 BytesOffset>
    inline constexpr T* at()
    {
        return const_cast<T*>(
            static_cast<const VertexAttribState&>(*this).at<T, BytesOffset>()
        );
    }

    template <u16 Flags, u16 Attrib>
    inline constexpr auto get()
    {
        constexpr u32 offset = vertex_attrib_offset<Flags, Attrib>();

        if constexpr (Attrib == VERTEX_COLOR)
        {
            return at<VertexAttribState::PackedColorType, offset>();
        };

        if constexpr (Attrib == VERTEX_NORMAL)
        {
            return at<VertexAttribState::PackedNormalType, offset>();
        };

        if constexpr (Attrib == VERTEX_TEXCOORD)
        {
            return at<VertexAttribState::PackedTexcoordType, offset>();
        };

        if constexpr (Attrib == VERTEX_TEXCOORD_F32)
        {
            return at<VertexAttribState::FullTexcoordType, offset>();
        }
    }
};

template <u16 Flags>
constexpr u32 vertex_attribs_size()
{
    u32 size = 0;

    if constexpr (!!(Flags & VERTEX_COLOR))
    {
        size += sizeof(VertexAttribState::PackedColorType);
    }

    if constexpr (!!(Flags & VERTEX_NORMAL))
    {
        size += sizeof(VertexAttribState::PackedNormalType);
    }

    if constexpr (!!(Flags & VERTEX_TEXCOORD))
    {
        if constexpr (!!(Flags & TEXCOORD_F32))
        {
            size += sizeof(VertexAttribState::FullTexcoordType);
        }
        else
        {
            size += sizeof(VertexAttribState::PackedTexcoordType);
        }
    }

    return size;
}

template <u16 Flags, u16 Attrib>
constexpr u32 vertex_attrib_offset()
{
    static_assert(
        Attrib == VERTEX_COLOR    ||
        Attrib == VERTEX_NORMAL   ||
        Attrib == VERTEX_TEXCOORD ||
        Attrib == VERTEX_TEXCOORD_F32,
        "Invalid `Attrib`."
    );

    static_assert(
        Attrib == (Flags & Attrib),
        "`Attrib` must be part of `Flags`."
    );

    u32 offset = 0;

    // Order: color, normal, texcooord.

    if constexpr (Attrib != VERTEX_COLOR && (Flags & VERTEX_COLOR))
    {
        offset += sizeof(VertexAttribState::PackedColorType);
    }

    if constexpr (Attrib != VERTEX_NORMAL && (Flags & VERTEX_NORMAL))
    {
        offset += sizeof(VertexAttribState::PackedNormalType);
    }

    return offset;
}

template <u16 Flags>
void store_color(VertexAttribState& state, u32 rgba)
{
    if constexpr (!!(Flags & VERTEX_COLOR))
    {
        *state.get<Flags, VERTEX_COLOR>() = bx::endianSwap(rgba);
    }
}

template <u16 Flags>
void store_normal(VertexAttribState& state, f32 nx, f32 ny, f32 nz)
{
    if constexpr (!!(Flags & VERTEX_NORMAL))
    {
        const f32 normalized[] =
        {
            nx * 0.5f + 0.5f,
            ny * 0.5f + 0.5f,
            nz * 0.5f + 0.5f,
        };

        bx::packRgb8(state.get<Flags, VERTEX_NORMAL>(), normalized);
    }
}

template <u16 Flags>
void store_texcoord(VertexAttribState& state, f32 u, f32 v)
{
    if constexpr (!!(Flags & VERTEX_TEXCOORD))
    {
        if constexpr (!!(Flags & TEXCOORD_F32))
        {
            *state.get<Flags, VERTEX_TEXCOORD_F32>() = HMM_Vec2(u, v);
        }
        else
        {
            const f32 elems[] = { u, v };
            bx::packRg16S(state.get<Flags, VERTEX_TEXCOORD>(), elems);
        }
    }
}

struct VertexAttribStateFuncSet
{
    void (* color)(VertexAttribState&, u32 rgba);

    void (* normal)(VertexAttribState&, f32 nx, f32 ny, f32 nz);

    void (* texcoord)(VertexAttribState&, f32 u, f32 v);
};

class VertexAttribStateFuncTable
{
    StaticArray<VertexAttribStateFuncSet, 16> m_table;

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

    inline VertexAttribStateFuncSet operator[](u16 flags) const
    {
        return m_table[index(flags)];
    }

private:
    static inline constexpr u32 index(u16 flags)
    {
        static_assert(
            VERTEX_ATTRIB_MASK >> VERTEX_ATTRIB_SHIFT == 0b0111 &&
            TEXCOORD_F32       >> 9                   == 0b1000,
            "Invalid index assumptions in `VertexAttribStateFuncTable::get_index_from_attribs`."
        );

        return
            ((flags & VERTEX_ATTRIB_MASK) >> VERTEX_ATTRIB_SHIFT) | // Bits 0..2.
            ((flags & TEXCOORD_F32      ) >> 9                  ) ; // Bit 3.
    }

    template <
        bool HasColor,
        bool HasNormal,
        bool HasTexCoord,
        bool HasTexCoordF32 = false
    >
    void variant()
    {
        if constexpr (HasTexCoord && !HasTexCoordF32)
        {
            variant<HasColor, HasNormal, HasTexCoord, true>();
        }

        constexpr u16 Flags =
            (HasColor       ? VERTEX_COLOR    : 0) |
            (HasNormal      ? VERTEX_NORMAL   : 0) |
            (HasTexCoord    ? VERTEX_TEXCOORD : 0) |
            (HasTexCoordF32 ? TEXCOORD_F32    : 0) ;

        VertexAttribStateFuncSet funcs;

        funcs.color    = store_color   <Flags>;
        funcs.normal   = store_normal  <Flags>;
        funcs.texcoord = store_texcoord<Flags>;

        m_table[index(Flags)] = funcs;
    }
};

} // namespace mnm
