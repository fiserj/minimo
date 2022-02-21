#pragma once

namespace mnm
{

// TODO : Move close to the mesh stuff.
static inline u16 mesh_attribs(u32 flags)
{
    return (flags & VERTEX_ATTRIB_MASK);
}

class VertexLayoutCache
{
    // This silliness is just to be able to zero-initialize (and avoid having
    // `StaticArray` support non-POD types).
    using VertexLayoutMemory = StaticArray<uint8_t, sizeof(bgfx::VertexLayout)>;

    StaticArray<      VertexLayoutMemory, 128> m_layouts;
    StaticArray<bgfx::VertexLayoutHandle, 128> m_handles;

public:
    void init()
    {
        m_layouts.fill({});
        m_handles.fill(BGFX_INVALID_HANDLE);

        //  +-------------- VERTEX_COLOR
        //  |  +----------- VERTEX_NORMAL
        //  |  |  +-------- VERTEX_TEXCOORD
        //  |  |  |
        variant<0, 0, 0>();
        variant<1, 0, 0>();
        variant<0, 1, 0>();
        variant<0, 0, 1>();
        variant<1, 1, 0>();
        variant<1, 0, 1>();
        variant<0, 1, 1>();
        variant<1, 1, 1>();
    }

    inline void clear()
    {
        for (u32 i = 0; i < m_handles.size(); i++)
        {
            destroy_if_valid(m_handles[i]);
        }
    }

    bgfx::VertexLayoutHandle resolve_alias(u32& inout_flags, u32 alias_flags)
    {
        const u32 orig_attribs  = mesh_attribs(inout_flags);
        const u32 alias_attribs = mesh_attribs(alias_flags);

        const u32 skips = orig_attribs & (~alias_attribs);
        const u32 idx   = index(orig_attribs, skips);

        inout_flags &= ~skips;

        return m_handles[idx];
    }

    inline const bgfx::VertexLayout& operator[](u32 flags) const
    {
        return *reinterpret_cast<const bgfx::VertexLayout*>(&m_layouts[index(flags)]);
    }

private:
    static inline constexpr u32 index(u32 attribs, u32 skips = 0)
    {
        static_assert(
            VERTEX_ATTRIB_MASK >>  VERTEX_ATTRIB_SHIFT       == 0b0000111 &&
           (VERTEX_ATTRIB_MASK >> (VERTEX_ATTRIB_SHIFT - 3)) == 0b0111000 &&
            TEXCOORD_F32       >>  6                         == 0b1000000,
            "Invalid index assumptions in `VertexLayoutCache::index`."
        );

        return
            ((skips   & VERTEX_ATTRIB_MASK) >>  VERTEX_ATTRIB_SHIFT     ) | // Bits 0..2.
            ((attribs & VERTEX_ATTRIB_MASK) >> (VERTEX_ATTRIB_SHIFT - 3)) | // Bits 3..5.
            ((attribs & TEXCOORD_F32      ) >>  6                       ) ; // Bit 6.
    }

    template <
        bool HasColor,
        bool HasNormal,
        bool HasTexCoord,
        bool HasTexCoordF32 = false
    >
    inline void variant()
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

        variant(Flags);
    }

    inline void variant(u32 flags)
    {
        constexpr u32 ATTRIB_MASK = VERTEX_ATTRIB_MASK | TEXCOORD_F32;
        
        variant(flags & ATTRIB_MASK, 0);
    }

    void variant(u32 attribs, u32 skips)
    {
        ASSERT(attribs == (attribs & (VERTEX_ATTRIB_MASK | TEXCOORD_F32)));
        ASSERT(skips == (skips & VERTEX_ATTRIB_MASK));
        ASSERT(skips != attribs || attribs == 0);
        ASSERT(skips == (skips & attribs));

        bgfx::VertexLayout layout;
        layout.begin();

        if (attribs == VERTEX_POSITION)
        {
            layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
        }

        if (!!(skips & VERTEX_COLOR))
        {
            layout.skip(4 * sizeof(u8));
        }
        else if (!!(attribs & VERTEX_COLOR))
        {
            layout.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
        }

        if (!!(skips & VERTEX_NORMAL))
        {
            layout.skip(4 * sizeof(u8));
        }
        else if (!!(attribs & VERTEX_NORMAL))
        {
            layout.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true);
        }

        if (!!(skips & VERTEX_TEXCOORD))
        {
            layout.skip(2 * (!!(attribs & TEXCOORD_F32) ? sizeof(f32) : sizeof(int16_t)));
        }
        else if (!!(attribs & VERTEX_TEXCOORD))
        {
            if (!!(attribs & TEXCOORD_F32))
            {
                layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
            }
            else
            {
                layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true);
            }
        }

        layout.end();
        ASSERT(layout.getStride() % 4 == 0);

        const u16 idx = index(attribs, skips);
        ASSERT(!bgfx::isValid(m_handles[idx]));

        m_layouts[idx] = *reinterpret_cast<VertexLayoutMemory*>(&layout);
        m_handles[idx] = bgfx::createVertexLayout(layout);

        // Add variants with skipped attributes (for aliasing).
        if (attribs && !skips)
        {
            for (skips = VERTEX_COLOR; skips < (attribs & VERTEX_ATTRIB_MASK); skips++)
            {
                if ((attribs & VERTEX_ATTRIB_MASK) != (skips & VERTEX_ATTRIB_MASK) && (attribs & skips) == skips)
                {
                    variant(attribs, skips);
                }
            }
        }
    }
};

} // namespace mnm
