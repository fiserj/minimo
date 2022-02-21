#pragma once

namespace mnm
{

struct Texture
{
    bgfx::TextureFormat::Enum   format      = bgfx::TextureFormat::Count;
    bgfx::BackbufferRatio::Enum ratio       = bgfx::BackbufferRatio::Count;
    u32                         read_frame  = UINT32_MAX;
    u16                         width       = 0;
    u16                         height      = 0;
    bgfx::TextureHandle         blit_handle = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle         handle      = BGFX_INVALID_HANDLE;

    void destroy()
    {
        ASSERT(bgfx::isValid(blit_handle) <= bgfx::isValid(handle));

        if (bgfx::isValid(blit_handle))
        {
            bgfx::destroy(blit_handle);
        }

        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
            *this = {};
        }
    }
};

class TextureCache
{
public:
    void clear()
    {
        MutexScope lock(m_mutex);

        for (u32 i = 0; i < m_textures.size(); i++)
        {
            m_textures[i].destroy();
        }
    }

    void add_texture(u16 id, u16 flags, u16 width, u16 height, u16 stride, const void* data)
    {
        ASSERT(id < m_textures.size());

        MutexScope lock(m_mutex);

        Texture& texture = m_textures[id];
        texture.destroy();

        static const u64 sampling_flags[] =
        {
            BGFX_SAMPLER_NONE,
            BGFX_SAMPLER_POINT,
        };

        static const u64 border_flags[] =
        {
            BGFX_SAMPLER_NONE,
            BGFX_SAMPLER_UVW_MIRROR,
            BGFX_SAMPLER_UVW_CLAMP,
        };

        static const u64 target_flags[] =
        {
            BGFX_TEXTURE_NONE,
            BGFX_TEXTURE_RT,
        };

        static const struct Format
        {
            u32                  size;
            bgfx::TextureFormat::Enum type;

        } formats[] =
        {
            { 4, bgfx::TextureFormat::RGBA8 },
            { 1, bgfx::TextureFormat::R8    },
            { 0, bgfx::TextureFormat::D24S8 },
            { 0, bgfx::TextureFormat::D32F  },
        };

        const Format format = formats[(flags & TEXTURE_FORMAT_MASK) >> TEXTURE_FORMAT_SHIFT];

        bgfx::BackbufferRatio::Enum ratio = bgfx::BackbufferRatio::Count;

        if (width >= SIZE_EQUAL && width <= SIZE_DOUBLE && width == height)
        {
            ratio = bgfx::BackbufferRatio::Enum(width - SIZE_EQUAL);
        }

        const bgfx::Memory* memory = nullptr;

        if (data && format.size > 0 && ratio == bgfx::BackbufferRatio::Count)
        {
            if (stride == 0 || stride == width * format.size)
            {
                memory = bgfx::copy(data, width * height * format.size);
            }
            else
            {
                const u8* src = static_cast<const u8*>(data);
                u8*       dst = memory->data;

                for (u16 y = 0; y < height; y++)
                {
                    (void)memcpy(dst, src, width * format.size);

                    src += stride;
                    dst += width * format.size;
                }
            }
        }

        const u64 texture_flags =
            sampling_flags[(flags & TEXTURE_SAMPLING_MASK) >> TEXTURE_SAMPLING_SHIFT] |
            border_flags  [(flags & TEXTURE_BORDER_MASK  ) >> TEXTURE_BORDER_SHIFT  ] |
            target_flags  [(flags & TEXTURE_TARGET_MASK  ) >> TEXTURE_TARGET_SHIFT  ] ;

        if (ratio == bgfx::BackbufferRatio::Count)
        {
            texture.handle = bgfx::createTexture2D(width, height, false, 1, format.type, texture_flags, memory);
        }
        else
        {
            ASSERT(!memory);
            texture.handle = bgfx::createTexture2D(ratio, false, 1, format.type, texture_flags);
        }
        ASSERT(bgfx::isValid(texture.handle));

        texture.format = format.type;
        texture.ratio  = ratio;
        texture.width  = width;
        texture.height = height;
    }

    void destroy_texture(u16 id)
    {
        ASSERT(id < m_textures.size());

        MutexScope lock(m_mutex);

        m_textures[id].destroy();
    }

    void schedule_read(u16 id, bgfx::ViewId pass, bgfx::Encoder* encoder, void* data)
    {
        MutexScope lock(m_mutex);

        Texture& texture = m_textures[id];
        ASSERT(bgfx::isValid(texture.handle));

        if (!bgfx::isValid(texture.blit_handle))
        {
            constexpr u64 flags =
                BGFX_TEXTURE_BLIT_DST  |
                BGFX_TEXTURE_READ_BACK |
                BGFX_SAMPLER_MIN_POINT |
                BGFX_SAMPLER_MAG_POINT |
                BGFX_SAMPLER_MIP_POINT |
                BGFX_SAMPLER_U_CLAMP   |
                BGFX_SAMPLER_V_CLAMP   ;

            texture.blit_handle = texture.ratio == bgfx::BackbufferRatio::Count
                ? texture.blit_handle = bgfx::createTexture2D(texture.width, texture.height, false, 1, texture.format, flags)
                : texture.blit_handle = bgfx::createTexture2D(texture.ratio                , false, 1, texture.format, flags);

            ASSERT(bgfx::isValid(texture.blit_handle));
        }

        encoder->blit(pass, texture.blit_handle, 0, 0, texture.handle);

        texture.read_frame = bgfx::readTexture(texture.blit_handle, data);
    }

    inline const Texture& operator[](u16 id) const { return m_textures[id]; }

private:
    Mutex                              m_mutex;
    StaticArray<Texture, MAX_TEXTURES> m_textures;
};

} // namespace mnm
