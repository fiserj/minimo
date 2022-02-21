#pragma once

namespace mnm
{

// TODO : Put into `base` or replace with something else.
template <typename Key, typename T>
using HashMap = std::unordered_map<Key, T>;

class Atlas
{
public:
    inline f32 font_size() const { return m_font_size; }

    inline bool is_free() const { return m_flags & ATLAS_FREE; }

    inline bool is_locked() const { return m_locked; }

    inline bool is_updatable() const { return m_flags & ATLAS_ALLOW_UPDATE; }

    inline bool is_monospaced() const { return m_flags & ATLAS_MONOSPACED; }

    inline bool does_not_require_thread_safety() const { return m_flags & ATLAS_NOT_THREAD_SAFE; }

    void reset(u16 texture, u16 flags, const void* font, f32 size, TextureCache& textures)
    {
        MutexScope lock(m_mutex);

        if (m_texture != UINT16_MAX)
        {
            textures.destroy_texture(m_texture);
        }

        // NOTE : It'd be much nicer to be able to call `*this = {}`, but we
        //        can't because of the mutex :-(.
        m_requests   .clear();
        m_pack_rects .clear();
        m_pack_nodes .clear();
        m_char_quads .clear();
        m_codepoints .clear();
        m_bitmap_data.clear();

        m_font_info     = {};
        m_pack_ctx      = {};
        m_bitmap_width  = 0;
        m_bitmap_height = 0;
        m_padding       = 1; // TODO : Padding should probably reflect whether an SDF atlas is required.
        m_locked        = false;
        m_font_size     = size;
        m_texture       = texture;
        m_flags         = flags;

        // TODO : Check return value.
        (void)stbtt_InitFont(&m_font_info, static_cast<const u8*>(font), 0);

        if (const int table = stbtt__find_table(m_font_info.data, m_font_info.fontstart, "OS/2"))
        {
            if (ttUSHORT(m_font_info.data + table     ) >= 1 && // Version.
                ttBYTE  (m_font_info.data + table + 32) == 2 && // PANOSE / Kind == `Latin Text`.
                ttBYTE  (m_font_info.data + table + 35) == 9)   // PANOSE / bProportion == "Monospaced"
            {
                flags |= ATLAS_MONOSPACED;
            }
        }
    }

    void add_glyph_range(u32 first, u32 last)
    {
        if (!is_updatable() && is_locked())
        {
            ASSERT(false && "Atlas is not updatable.");
            return;
        }

        ASSERT(last >= first);

        size_t i = m_requests.size;
        m_requests.resize(i + size_t(last - first + 1));

        for (u32 codepoint = first; codepoint <= last; codepoint++, i++)
        {
            if (!m_codepoints.count(codepoint))
            {
                m_requests[i] = codepoint;
            }
        }
    }

    void add_glyphs_from_string(const char* start, const char* end)
    {
        if (!is_updatable() && is_locked())
        {
            ASSERT(false && "Atlas is not updatable.");
            return;
        }

        u32 codepoint;
        u32 state = 0;

        for (const char* string = start; end ? string < end : *string; string++)
        {
            if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const u8*>(string), codepoint))
            {
                if (!m_codepoints.count(codepoint))
                {
                    m_requests.push(codepoint);
                }
            }
        }

        ASSERT(state == UTF8_ACCEPT);
    }

    // TODO : This should return success bool.
    void update(TextureCache& texture_cache)
    {
        ASSERT(is_updatable() || !is_locked());

        if (m_requests.is_empty() || is_locked())
        {
            return;
        }

        // TODO : Remove use of STL ?
        {
            std::sort(m_requests.data, m_requests.data + m_requests.size);
            const u32* end = std::unique(m_requests.data, m_requests.data + m_requests.size);

            ASSERT(end > m_requests.data && end <= m_requests.data + m_requests.size);

            m_requests.resize(end - m_requests.data);
        }

        ASSERT(m_pack_rects.size == m_char_quads.size);

        const size_t count  = m_requests  .size;
        const size_t offset = m_pack_rects.size;

        m_pack_rects.resize(offset + count, stbrp_rect       {});
        m_char_quads.resize(offset + count, stbtt_packedchar {});

        stbtt_pack_context ctx            = {};
        ctx.padding                       = m_padding;
        ctx.h_oversample                  = horizontal_oversampling();
        ctx.v_oversample                  = vertical_oversampling  ();
        ctx.skip_missing                  = 0;

        stbtt_pack_range range            = {};
        range.font_size                   = font_scale();
        range.h_oversample                = horizontal_oversampling();
        range.v_oversample                = vertical_oversampling  ();
        range.chardata_for_range          = m_char_quads.data + offset;
        range.array_of_unicode_codepoints = reinterpret_cast<int*>(m_requests.data);
        range.num_chars                   = int(m_requests.size);

        (void)stbtt_PackFontRangesGatherRects(
            &ctx,
            &m_font_info,
            &range,
            1,
            m_pack_rects.data + offset
        );

        u32 pack_size[] = { m_bitmap_width, m_bitmap_height };
        pack_rects(offset, count, pack_size);

        if (m_bitmap_width  != pack_size[0] ||
            m_bitmap_height != pack_size[1])
        {
            DynamicArray<u8> data;
            data.resize(pack_size[0] * pack_size[1], 0);

            for (u32 y = 0, src_offset = 0, dst_offset = 0; y < m_bitmap_height; y++)
            {
                memcpy(data.data + dst_offset, m_bitmap_data.data + src_offset, m_bitmap_width);

                src_offset += m_bitmap_width;
                dst_offset += pack_size[0];
            }

            m_bitmap_width  = pack_size[0];
            m_bitmap_height = pack_size[1];
            m_bitmap_data.swap(data);
        }

        ctx.width           = m_bitmap_width;
        ctx.height          = m_bitmap_height;
        ctx.stride_in_bytes = m_bitmap_width;
        ctx.pixels          = m_bitmap_data.data;

        // TODO : Utilize the return value.
        (void)stbtt_PackFontRangesRenderIntoRects(
            &ctx,
            &m_font_info,
            &range,
            1,
            m_pack_rects.data + offset
        );

        // TODO : We should only update the texture if the size didn't change.
        texture_cache.add_texture(
            m_texture,
            TEXTURE_R8,
            m_bitmap_width,
            m_bitmap_height,
            0,
            m_bitmap_data.data
        );

        for (size_t i = 0; i < m_requests.size; i++)
        {
            m_codepoints.insert({ m_requests[i], u16(offset + i) });
        }

        m_requests.clear();

        if (!is_updatable())
        {
            m_locked = true;
        }
    }

    using QuadPackFunc = void (*)(const stbtt_packedchar&, f32, f32, f32&, stbtt_aligned_quad&);

    QuadPackFunc get_quad_pack_func(bool align_to_integer, bool y_axis_down)
    {
        // TODO : Move to class-member level?
        static const QuadPackFunc s_dispatch_table[8] =
        {
            //        +------------------- YAxisDown
            //        |  +---------------- UseTexCoord
            //        |  |  +------------- AlignToInteger
            //        |  |  |
            pack_quad<0, 0, 0>,
            pack_quad<0, 0, 1>,
            pack_quad<0, 1, 0>,
            pack_quad<0, 1, 1>,
            pack_quad<1, 0, 0>,
            pack_quad<1, 0, 1>,
            pack_quad<1, 1, 0>,
            pack_quad<1, 1, 1>,
        };

        const bool use_tex_coord = !is_updatable();
        const int  index         = 
            (align_to_integer ? 0b001 : 0) |
            (use_tex_coord    ? 0b010 : 0) |
            (y_axis_down      ? 0b100 : 0) ;

        return s_dispatch_table[index];
    }

    bool get_text_size
    (
        const char* start,
        const char* end,
        f32       line_height_factor,
        f32*      out_width,
        f32*      out_height
    )
    {
        f32       line_width  = 0.0f;
        const f32 line_height = roundf(font_size() * line_height_factor);
        f32       box_width   = 0.0f;
        f32       box_height  = font_size();
        u32    codepoint   = 0;
        u32    state       = 0;

        for (const char* string = start; end ? string < end : *string; string++)
        {
            if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const u8*>(string), codepoint))
            {
                if (codepoint == '\n') // TODO : Other line terminators?
                {
                    box_height += line_height;
                    box_width   = std::max(box_width, line_width);
                    line_width  = 0.0f;

                    continue;
                }

                const auto it = m_codepoints.find(codepoint);

                if (it == m_codepoints.end())
                {
                    return false;
                }

                // TODO : Needs to reflect `align_to_integer`.
                line_width += m_char_quads[it->second].xadvance;
            }
        }

        ASSERT(state == UTF8_ACCEPT);

        box_width = std::max(box_width, line_width);

        if (out_width ) { *out_width  = box_width ; }
        if (out_height) { *out_height = box_height; }

        return true;
    }

    // Two-pass:
    // 1) Gather info about text, signal missing glyphs.
    // 2) Submit quads to the recorder.
    bool lay_text
    (
        const char*   start,
        const char*   end,
        f32         line_height_factor,
        u16      h_alignment,
        u16      v_alignment,
        bool          align_to_integer,
        bool          y_axis_down,
        const Mat4&   transform,
        MeshRecorder& out_recorder
    )
    {
        DynamicArray<f32> line_widths; // TODO : Candidate for stack-based allocator usage.
        const f32   line_sign         = y_axis_down ? 1.0f : -1.0f;
        const f32   line_height       = roundf(font_size() * line_height_factor);
        f32         line_width        = 0.0f;
        f32         box_width         = 0.0f;
        f32         box_height        = font_size();
        u32      codepoint         = 0;
        u32      state             = 0;
        const bool    needs_line_widths = h_alignment != TEXT_H_ALIGN_LEFT;

        // Pass 1: Gather info about text, signal missing glyphs.
        for (const char* string = start; end ? string < end : *string; string++)
        {
            if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const u8*>(string), codepoint))
            {
                if (codepoint == '\n') // TODO : Other line terminators?
                {
                    if (needs_line_widths)
                    {
                        line_widths.push(line_width);
                    }

                    box_height += line_height;
                    box_width   = std::max(box_width, line_width);
                    line_width  = 0.0f;

                    continue;
                }

                const auto it = m_codepoints.find(codepoint);

                if (it == m_codepoints.end())
                {
                    if (is_updatable())
                    {
                        return false;
                    }
                    else
                    {
                        // TODO : Probably just print some warning and skip the
                        //        glyph (try using the atlas' "missing" glyph).
                        ASSERT(false && "Atlas is immutable.");
                    }
                }

                // TODO : Needs to reflect `align_to_integer`.
                line_width += m_char_quads[it->second].xadvance;
            }
        }

        ASSERT(state == UTF8_ACCEPT);

        if (needs_line_widths)
        {
            line_widths.push(line_width);
        }

        box_width = std::max(box_width, line_width);

        if (box_width == 0.0f)
        {
            return true;
        }

        // Pass 2: Submit quads to the recorder.
        Vec3     offset   = HMM_Vec3(0.0f, 0.0f, 0.0f);
        u16 line_idx = 0;

        switch (v_alignment)
        {
        case TEXT_V_ALIGN_BASELINE:
            offset.Y = line_sign * (font_size() - box_height);
            break;
        case TEXT_V_ALIGN_MIDDLE:
            offset.Y = roundf(line_sign * (box_height * -0.5f + font_size()));
            break;
        case TEXT_V_ALIGN_CAP_HEIGHT:
            offset.Y = line_sign * font_size();
            break;
        default:;
        }

        const QuadPackFunc pack_func = get_quad_pack_func(align_to_integer, y_axis_down);

        for (const char* string = start; end ? string < end : *string; line_idx++)
        {
            ASSERT(!end || string < end);

            switch (h_alignment)
            {
            case TEXT_H_ALIGN_CENTER:
                offset.X = line_widths[line_idx] * -0.5f;
                break;
            case TEXT_H_ALIGN_RIGHT:
                offset.X = -line_widths[line_idx];
                break;
            default:;
            }

            string = record_quads
            (
                string,
                end,
                pack_func,
                transform * HMM_Translate(offset),
                out_recorder
            );

            offset.Y += line_sign * line_height;
        }

        return true;
    }

private:
    template <bool YAxisDown, bool UseTexCoord, bool AlignToInteger>
    static void pack_quad
    (
        const stbtt_packedchar& char_info,
        f32                   inv_width,
        f32                   inv_height,
        f32&                  inout_xpos,
        stbtt_aligned_quad&     out_quad
    )
    {
        if constexpr (AlignToInteger)
        {
            const f32 x = floorf(inout_xpos + char_info.xoff + 0.5f);
            const f32 y = floorf(             char_info.yoff + 0.5f);

            out_quad.x0 = x;
            out_quad.x1 = x + char_info.xoff2 - char_info.xoff;

            if constexpr (YAxisDown)
            {
                out_quad.y0 = y;
                out_quad.y1 = y + char_info.yoff2 - char_info.yoff;
            }
            else
            {
                out_quad.y0 = -y;
                out_quad.y1 = -y - char_info.yoff2 + char_info.yoff;
            }
        }
        else
        {
            out_quad.x0 = inout_xpos + char_info.xoff;
            out_quad.x1 = inout_xpos + char_info.xoff2;

            if constexpr (YAxisDown)
            {
                out_quad.y0 = char_info.yoff;
                out_quad.y1 = char_info.yoff2;
            }
            else
            {
                out_quad.y0 = -char_info.yoff;
                out_quad.y1 = -char_info.yoff2;
            }
        }

        if constexpr (UseTexCoord)
        {
            out_quad.s0 = char_info.x0 * inv_width;
            out_quad.t0 = char_info.y0 * inv_height;
            out_quad.s1 = char_info.x1 * inv_width;
            out_quad.t1 = char_info.y1 * inv_height;
        }
        else
        {
            out_quad.s0 = char_info.x0;
            out_quad.t0 = char_info.y0;
            out_quad.s1 = char_info.x1;
            out_quad.t1 = char_info.y1;
        }

        inout_xpos += char_info.xadvance;
    }

    inline const char* record_quads(const char* start, const char* end, const QuadPackFunc& pack_func, const Mat4& transform, MeshRecorder& recorder)
    {
        if (!is_updatable() || does_not_require_thread_safety())
        {
            return record_quads_without_lock(start, end, pack_func, transform, recorder);
        }
        else
        {
            MutexScope lock(m_mutex);

            return record_quads_without_lock(start, end, pack_func, transform, recorder);
        }
    }

    const char* record_quads_without_lock(const char* start, const char* end, const QuadPackFunc& pack_func, const Mat4& transform, MeshRecorder& recorder)
    {
        // NOTE : This routine assumes all needed glyphs are loaded!

        u32           codepoint;
        u32           state      = 0;
        const f32        inv_width  = 1.0f / f32(m_bitmap_width );
        const f32        inv_height = 1.0f / f32(m_bitmap_height);
        f32              x          = 0.0f;
        stbtt_aligned_quad quad       = {};

        for (; end ? start < end : *start; start++)
        {
            if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const u8*>(start), codepoint))
            {
                if (codepoint == '\n') // TODO : Other line terminators?
                {
                    start++;
                    break;
                }

                const auto it = m_codepoints.find(codepoint);
                ASSERT(it != m_codepoints.end());

                (*pack_func)(m_char_quads[it->second], inv_width, inv_height, x, quad);

                recorder.texcoord(                      quad.s0, quad.t0);
                recorder.vertex  ((transform * HMM_Vec4(quad.x0, quad.y0, 0.0f, 1.0f)).XYZ);

                recorder.texcoord(                      quad.s0, quad.t1);
                recorder.vertex  ((transform * HMM_Vec4(quad.x0, quad.y1, 0.0f, 1.0f)).XYZ);

                recorder.texcoord(                      quad.s1, quad.t1);
                recorder.vertex  ((transform * HMM_Vec4(quad.x1, quad.y1, 0.0f, 1.0f)).XYZ);

                recorder.texcoord(                      quad.s1, quad.t0);
                recorder.vertex  ((transform * HMM_Vec4(quad.x1, quad.y0, 0.0f, 1.0f)).XYZ);
            }
        }

        ASSERT(state == UTF8_ACCEPT);

        return start;
    }

    int16_t cap_height() const
    {
        if (const int table = stbtt__find_table(m_font_info.data, m_font_info.fontstart, "OS/2"))
        {
            if (ttUSHORT(m_font_info.data + table) >= 2) // Version.
            {
                return ttSHORT(m_font_info.data + table + 88); // sCapHeight.
            }
        }

        // TODO : Estimate cap height from capital `H` bounding box?
        ASSERT(false && "Can't determine cap height.");

        return 0;
    }

    inline f32 font_scale() const
    {
        int ascent, descent;
        stbtt_GetFontVMetrics(&m_font_info, &ascent, &descent, nullptr);

        return (ascent - descent) * m_font_size / cap_height();
    }

    inline u8 horizontal_oversampling() const
    {
        constexpr u32 H_OVERSAMPLE_MASK  = ATLAS_H_OVERSAMPLE_2X | ATLAS_H_OVERSAMPLE_3X | ATLAS_H_OVERSAMPLE_4X;
        constexpr u32 H_OVERSAMPLE_SHIFT = 3;

        const u8 value = u8(((m_flags & H_OVERSAMPLE_MASK) >> H_OVERSAMPLE_SHIFT) + 1);
        ASSERT(value >= 1 && value <= 4);

        return value;
    }

    inline u8 vertical_oversampling() const
    {
        constexpr u32 V_OVERSAMPLE_MASK  = ATLAS_V_OVERSAMPLE_2X;
        constexpr u32 V_OVERSAMPLE_SHIFT = 6;

        const u8 value = u8(((m_flags & V_OVERSAMPLE_MASK) >> V_OVERSAMPLE_SHIFT) + 1);
        ASSERT(value >= 1 && value <= 2);

        return value;
    }

    bool pick_next_size(u32 min_area, u32* inout_pack_size) const
    {
        const u32 max_size = bgfx::getCaps()->limits.maxTextureSize;
        u32       size[2]  = { 64, 64 };

        for (int j = 0;; j = (j + 1) % 2)
        {
            if (size[0] > inout_pack_size[0] || size[1] > inout_pack_size[1])
            {
                const u32 area = (size[0] - m_padding) * (size[1] - m_padding);

                if (area >= min_area * 1.075f) // 7.5 % extra space, as the packing won't be perfect.
                {
                    break;
                }
            }

            if (size[0] == max_size && size[1] == max_size)
            {
                ASSERT(false && "Maximum atlas size reached."); // TODO : Convert to `WARNING`.
                return false;
            }

            size[j] *= 2;
        }

        inout_pack_size[0] = size[0];
        inout_pack_size[1] = size[1];

        return true;
    }

    void pack_rects(size_t offset, size_t count, u32* inout_pack_size)
    {
        u32    min_area   = 0;
        const f32 extra_area = 1.05f;

        for (u32 i = 0; i < m_pack_rects.size; i++)
        {
            min_area += u32(m_pack_rects[i].w * m_pack_rects[i].h);
        }

        min_area = u32(f32(min_area) * extra_area);

        for (;;)
        {
            if (inout_pack_size[0] > 0 && inout_pack_size[1] > 0)
            {
                // TODO : It's probably possible to revert the packing context
                //        without having to making its full copy beforehand.
                stbrp_context      ctx = m_pack_ctx;
                DynamicArray<stbrp_node> nodes(m_pack_nodes); // TODO : Candidate for stack-based allocator usage.

                // NOTE : This only packs the new rectangles.
                if (1 == stbrp_pack_rects(
                    &m_pack_ctx,
                    m_pack_rects.data + offset,
                    int(count)
                ))
                {
                    break;
                }
                else
                {
                    m_pack_ctx   = ctx;
                    m_pack_nodes = nodes;
                }

                // TODO : We could adjust `offset` and `count` so that the rects
                //        that were successfully packed would be skipped in next
                //        resizing attempt, but we'd have to reorder them.
            }

            if (pick_next_size(min_area, inout_pack_size))
            {
                if (m_pack_ctx.num_nodes == 0)
                {
                    m_pack_nodes.resize(inout_pack_size[0] - m_padding);

                    stbrp_init_target(
                        &m_pack_ctx,
                        inout_pack_size[0] - m_padding,
                        inout_pack_size[1] - m_padding,
                        m_pack_nodes.data,
                        int(m_pack_nodes.size)
                    );
                }
                else
                {
                    // Atlas size changed (and so did the packing rectangle).
                    patch_stbrp_context(inout_pack_size[0], inout_pack_size[1]);
                }
            }
            else
            {
                ASSERT(false && "Maximum atlas size reached and all glyphs "
                    "still can't be packed."); // TODO : Convert to `WARNING`.
                break;
            }
        }
    }

#ifndef NDEBUG
    static void check_stbrp_context_validity(const stbrp_context& ctx, const DynamicArray<stbrp_node>& nodes)
    {
        const auto check_node =[&](const stbrp_node* node)
        {
            const bool is_in_range = node >= nodes.data && node < nodes.data + nodes.size;
            const bool is_extra    = node == &ctx.extra[0] || node == &ctx.extra[1];
            const bool is_null     = node == nullptr;

            ASSERT(is_in_range || is_extra || is_null);
        };

        const auto count_nodes = [](const stbrp_node* node, bool check_zero)
        {
            int count = 0;

            while (node)
            {
                ASSERT(!check_zero || (node->x == 0 && node->y == 0));

                node = node->next;
                count++;
            }

            return count;
        };

        const int active_nodes_count = count_nodes(ctx.active_head, false);
        const int free_nodes_count   = count_nodes(ctx.free_head  , true );

        ASSERT(2 + ctx.num_nodes == active_nodes_count + free_nodes_count);

        check_node(ctx.active_head);
        check_node(ctx.active_head->next);

        check_node(ctx.free_head);
        check_node(ctx.free_head->next);

        check_node(ctx.extra[0].next);
        check_node(ctx.extra[1].next);

        for (u32 i = 0; i < nodes.size; i++)
        {
            check_node(nodes[i].next);
        }
    }
#endif // NDEBUG

    void patch_stbrp_context(u32 width, u32 height)
    {
#ifndef NDEBUG
        check_stbrp_context_validity(m_pack_ctx, m_pack_nodes);
#endif

        // When changing only height, number of nodes or the sentinel node don't
        // change.
        if (width - m_padding == u32(m_pack_ctx.width))
        {
            m_pack_ctx.height = int(height - m_padding);

            return;
        }

        // TODO : Use scratch / frame allocation.
        DynamicArray<stbrp_node> nodes;
        nodes.resize(width - m_padding);

        const auto find_node = [&](stbrp_node* node)
        {
            // Node can either point to one of the given array members, the two
            // `extra` nodes allocated within the `stbrp_context` structure, or
            // be `NULL`.
            const uintptr_t offset = node - m_pack_nodes.data; // Fine even if `nullptr`.

            // We're intentionally not adjusting the nodes that point to one of
            // the context's `extra` nodes, so that we don't have to repeatedly
            // patch them when the context would be swapped.
            return offset < m_pack_nodes.size
                ? &nodes[offset]
                : node;
        };

        stbrp_context ctx = {};

        stbrp_init_target(
            &ctx,
            int(width  - m_padding),
            int(height - m_padding),
            nodes.data,
            int(nodes.size)
        );

        ctx.active_head   = find_node(m_pack_ctx.active_head  );
        ctx.free_head     = find_node(m_pack_ctx.free_head    );
        ctx.extra[0].next = find_node(m_pack_ctx.extra[0].next);
        ctx.extra[0].x    =           m_pack_ctx.extra[0].x    ;
        ctx.extra[0].y    =           m_pack_ctx.extra[0].y    ;
        // NOTE : Node `extra[1]` is a sentinel, so no need to patch it.

        // TODO : It's possible that some special handling will be necessary if
        //        the old context ran out of free nodes, but we're increasing
        //        width. So do some special logic here (will need artifical test
        //        as it's quite unlikely, I think).
        if (!m_pack_ctx.free_head)
        {
            ASSERT(false && "Not implemented.");
        }

        for (size_t i = 0; i < m_pack_nodes.size - 1; i++)
        {
            nodes[i].x    =           m_pack_nodes[i].x;
            nodes[i].y    =           m_pack_nodes[i].y;
            nodes[i].next = find_node(m_pack_nodes[i].next);
        }

        m_pack_ctx = ctx; // We can do this safely as no nodes point to `ctx.extra`.
        m_pack_nodes.swap(nodes);

#ifndef NDEBUG
        check_stbrp_context_validity(m_pack_ctx, m_pack_nodes);
#endif
    }

private:
    Mutex                       m_mutex;

    stbtt_fontinfo              m_font_info     = {};
    f32                       m_font_size     = 0.0f; // Cap height, in pixels.

    DynamicArray<u32>            m_requests;

    // Packing.
    stbrp_context               m_pack_ctx      = {};
    DynamicArray<stbrp_rect>          m_pack_rects;
    DynamicArray<stbrp_node>          m_pack_nodes;

    // Packed data.
    DynamicArray<stbtt_packedchar>    m_char_quads;
    HashMap<u32, u16> m_codepoints;

    // Bitmap.
    DynamicArray<u8>             m_bitmap_data;
    u16                    m_bitmap_width  = 0;
    u16                    m_bitmap_height = 0;

    u16                    m_texture       = UINT16_MAX;
    u16                    m_flags         = ATLAS_FREE;
    u8                     m_padding       = 1;
    bool                        m_locked        = false;
};

} // namespace mnm

