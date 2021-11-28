#pragma once

namespace gui
{

// -----------------------------------------------------------------------------
// LIMITS
// -----------------------------------------------------------------------------

constexpr uint32_t MAX_RECT_BUFFER_SIZE = 512;

constexpr uint32_t MAX_TEXT_BUFFER_SIZE = 4096;


// -----------------------------------------------------------------------------
// DATA TYPES AND STRUCTURES
// -----------------------------------------------------------------------------

enum State
{
    STATE_COLD,
    STATE_HOT,
    STATE_ACTIVE,
};

struct Rect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    inline float width() const
    {
        return x1 - x0;
    }

    inline float height() const
    {
        return y1 - y0;
    }

    bool is_hovered() const
    {
        const float x = mouse_x();
        const float y = mouse_y();

        return x >= x0 && x < x1 && y >= y0 && y < y1;
    }
};

struct ColorRect
{
    uint32_t color = 0x00000000;
    Rect     rect;
};

struct IdStack
{
    union
    {
        struct
        {
            uint8_t  size;
            uint8_t  stack[7];
        };

        uint64_t     hash = 0;
    };

    inline bool operator==(IdStack other) const
    {
        return hash == other.hash;
    }

    inline void clear()
    {
        hash = 0;
    }

    inline bool is_empty() const
    {
        return size == 0;
    }

    inline uint8_t top() const
    {
        ASSERT(size > 0);

        return stack[size - 1];
    }

    inline void push(uint8_t id)
    {
        ASSERT(size < 7);

        stack[size++] = id;
    }

    inline uint8_t pop()
    {
        const uint8_t value = top();

        // NOTE : We have to explicitly clear the popped value, so that the hash
        //        is consistent.
        stack[size-- - 1] = 0;

        return value;
    }

    IdStack copy_and_push(uint8_t id) const
    {
        IdStack copy = *this;
        copy.push(id);

        return copy;
    }
};

struct Resources
{
    int font_atlas              = 0;

    int framebuffer_glyph_cache = 0;

    int mesh_tmp_text           = 0;
    int mesh_gui_rects          = 0;
    int mesh_gui_text           = 0;

    int pass_glyph_cache        = 0;
    int pass_gui                = 0;

    int program_gui_text        = 0;

    int texture_glyph_cache     = 0;
    int texture_tmp_atlas       = 0;

    int uniform_text_info       = 0;
};

// TODO : (1) Add "unknown" glyph character.
//        (2) Add support for non-ASCII characters.
//        (3) Add support for on-demand atlas update.
struct GlyphCache
{
    int   texture_size = 0;
    int   glyph_cols   = 0;
    float glyph_width  = 0.0f; // In pixels.
    float glyph_height = 0.0f; // In pixels.

    inline float glyph_screen_width() const
    {
        return (glyph_width  - 1.0f) / dpi();
    }

    inline float glyph_screen_height() const
    {
        return glyph_height / dpi();
    }

    void rebuild(float cap_height, const Resources& res)
    {
        ASSERT(cap_height > 0.0f);

        begin_atlas(
            res.texture_tmp_atlas,
            ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE, // TODO : `ATLAS_ALLOW_UPDATE` seems to be broken again.
            res.font_atlas,
            cap_height * dpi()
        );
        glyph_range(0x20, 0x7e);
        end_atlas();

        text_size(res.texture_tmp_atlas, "X", 0, 1.0f, &glyph_width, &glyph_height);

        glyph_width  += 1.0f;
        glyph_height *= 2.0f;

        for (texture_size = 128; ; texture_size *= 2)
        {
            // TODO : Rounding and padding.
            glyph_cols = (int)(texture_size / glyph_width );
            const int rows = (int)(texture_size / glyph_height);

            if (glyph_cols * rows >= 95)
            {
                break;
            }
        }

        begin_text(
            res.mesh_tmp_text,
            res.texture_tmp_atlas,
            TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT
        );
        {
            color(0xffffffff);

            for (int i = 0; i < 95; i++)
            {
                const int x = i % glyph_cols;
                const int y = i / glyph_cols;

                identity();
                translate(x * glyph_width, (y + 0.25f) * glyph_height, 0.0f);

                char letter[2] = { (char)(i + 32), 0 };
                text(letter, 0);
            }
        }
        end_text();

        create_texture(
            res.texture_glyph_cache,
            TEXTURE_R8 | TEXTURE_CLAMP | TEXTURE_TARGET,
            texture_size,
            texture_size
        );

        begin_framebuffer(res.framebuffer_glyph_cache);
        texture(res.texture_glyph_cache);
        end_framebuffer();

        pass(res.pass_glyph_cache);

        framebuffer(res.framebuffer_glyph_cache);
        clear_color(0x000000ff); // TODO : If we dynamically update the cache, we only have to clear before the first draw.
        viewport(0, 0, texture_size, texture_size);

        identity();
        ortho(0.0f, (float)texture_size, (float)texture_size, 0.0f, 1.0f, -1.0f);
        projection();

        identity();
        mesh(res.mesh_tmp_text);
    }
};

struct TextBuffer
{
    uint32_t data[MAX_TEXT_BUFFER_SIZE] = { 0 }; // TODO : Dynamic memory ?
    uint32_t size                       =   0  ;
    uint32_t offset                     =   0  ;

    inline void clear()
    {
        size = 0;
    }

    void start(uint32_t color, float x, float y)
    {
        ASSERT(size + 4 < MAX_TEXT_BUFFER_SIZE);

        offset = size++;

        data[size++] = color;
        data[size++] = *(uint32_t*)&x;
        data[size++] = *(uint32_t*)&y;
    }

    inline void add(uint32_t index)
    {
        ASSERT(size < MAX_TEXT_BUFFER_SIZE);

        data[size++] = index;
    }

    inline void end()
    {
        data[offset] = size - offset - 4;
    }

    void submit(const GlyphCache& gc, const Resources& res)
    {
        if (size == 0)
        {
            return;
        }

        ASSERT(size >= 4);

        begin_mesh(res.mesh_gui_text, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR | NO_VERTEX_TRANSFORM);

        const float width  = gc.glyph_screen_width ();
        const float height = gc.glyph_screen_height();

        for (uint32_t i = 0; i < size;)
        {
            const uint32_t length =           data[i++];
            const uint32_t color  =           data[i++];
            float          x0     = *(float*)&data[i++];
            const float    y0     = *(float*)&data[i++];
            float          x1     = x0 + width;
            const float    y1     = y0 + height;

            ::color(color);

            for (uint32_t j = 0; j < length; j++, i++)
            {
                // TODO ? Pack also color and clip rectangle index (from a palette of, say, 16 colors) ?
                const float idx = (float)data[i] * 4.0f;

                vertex(x0, y0, idx + 0.0f);
                vertex(x0, y1, idx + 1.0f);
                vertex(x1, y1, idx + 2.0f);
                vertex(x1, y0, idx + 3.0f);

                x0  = x1;
                x1 += width;
            }
        }

        end_mesh();

        const float atlas_info[4] =
        {
            1.0f / gc.texture_size,
            (float)gc.glyph_cols,
            gc.glyph_width,
            gc.glyph_height,
        };

        identity();
        state   (STATE_BLEND_ALPHA | STATE_WRITE_RGB);
        uniform (res.uniform_text_info, atlas_info);
        texture (res.texture_glyph_cache);
        shader  (res.program_gui_text);
        mesh    (res.mesh_gui_text);

        offset = 0;
    }
};

struct RectBuffer
{
    ColorRect data[MAX_RECT_BUFFER_SIZE]; // TODO : Dynamic memory ?
    uint32_t  size = 0 ;

    inline void clear()
    {
        size = 0;
    }

    inline void add(const ColorRect& rect)
    {
        ASSERT(size < MAX_RECT_BUFFER_SIZE);

        data[size++] = rect;
    }

    void submit(const Resources& res)
    {
        if (size == 0)
        {
            return;
        }

        begin_mesh(res.mesh_gui_rects, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR | NO_VERTEX_TRANSFORM);
        {
            for (const ColorRect* rect = data, *end = data + size; rect < end; rect++)
            {
                color (rect->color);
                vertex(rect->rect.x0, rect->rect.y0, 0.0f);
                vertex(rect->rect.x0, rect->rect.y1, 0.0f);
                vertex(rect->rect.x1, rect->rect.y1, 0.0f);
                vertex(rect->rect.x1, rect->rect.y0, 0.0f);
            }
        }
        end_mesh();

        identity();
        state(STATE_WRITE_RGB);
        mesh (res.mesh_gui_rects);
    }
};

struct Context
{
    Resources   resources;
    IdStack     active_stack;
    IdStack     current_stack;
    TextBuffer  text_buffer;
    RectBuffer  rect_buffer;
    GlyphCache  glyph_cache;
    int         cursor          = CURSOR_ARROW;
    float       drag_start_x    = 0.0f;
    float       drag_start_y    = 0.0f;
    float       scroll_start_y  = 0.0f;
    float       font_cap_height = 8.0f; // In screen coordinates.

    void begin_frame()
    {
        if (dpi_changed())
        {
            glyph_cache.rebuild(font_cap_height, resources);
        }
    }

    void end_frame()
    {
        ASSERT(current_stack.is_empty());

        ::cursor(cursor);
        cursor = CURSOR_ARROW;

        if (!(mouse_down(MOUSE_LEFT) || mouse_held(MOUSE_LEFT)))
        {
            active_stack.clear();
        }

        pass(resources.pass_gui);

        identity();
        ortho(0.0f, width(), height(), 0.0f, 1.0f, -1.0f);
        projection();

        rect_buffer.submit(resources);
        rect_buffer.clear ();

        text_buffer.submit(glyph_cache, resources);
        text_buffer.clear ();
    }

    inline void push_id(uint8_t id)
    {
        current_stack.push(id);
    }

    inline void pop_id()
    {
        current_stack.pop();
    }

    inline bool none_active() const
    {
        return active_stack.is_empty();
    }

    inline bool is_active(uint8_t id) const
    {
        return active_stack == current_stack.copy_and_push(id);
    }

    inline void make_active(uint8_t id)
    {
        active_stack = current_stack.copy_and_push(id);
    }

    bool button_logic(uint8_t id, const Rect& rect, State& out_state)
    {
        out_state = STATE_COLD;

        if (rect.is_hovered() && none_active())
        {
            out_state = STATE_HOT;

            if (mouse_down(MOUSE_LEFT))
            {
                make_active(id);
            }
        }

        if (is_active(id))
        {
            out_state = STATE_ACTIVE;
        }

        return mouse_up(MOUSE_LEFT) && is_active(id) && rect.is_hovered();
    }

    bool drag_logic(uint8_t id, const Rect& rect, State& out_state, float& out_x, float& out_y)
    {
        out_state = STATE_COLD;

        if (rect.is_hovered() && none_active())
        {
            out_state = STATE_HOT;

            if (mouse_down(MOUSE_LEFT))
            {
                make_active(id);

                drag_start_x = out_x - mouse_x();
                drag_start_y = out_y - mouse_y();
            }
        }

        if (is_active(id))
        {
            out_state = STATE_ACTIVE;

            out_x = drag_start_x + mouse_x();
            out_y = drag_start_y + mouse_y();
        }

        return out_state != STATE_COLD;
    }

    static inline float remap_range(float in, float in_min, float in_max, float out_min, float out_max)
    {
        const float percent = (in - in_min) / (in_max - in_min);

        return bx::clamp(out_min + percent * (out_max - out_min), out_min, out_max);
    }

    bool scrollbar_logic(uint8_t id, const Rect& rect, State& out_state, float& out_handle_pos,
        float handle_size, float &out_val, float val_min, float val_max)
    {
        out_state = STATE_COLD;

        if (rect.is_hovered() && none_active())
        {
            out_state = STATE_HOT;

            if (mouse_down(MOUSE_LEFT))
            {
                make_active(id);

                out_handle_pos = remap_range(out_val, val_min, val_max, rect.y0, rect.y1 - handle_size);

                if (mouse_y() < out_handle_pos || mouse_y() > out_handle_pos + handle_size)
                {
                    out_handle_pos = mouse_y() - handle_size * 0.5f;
                }

                scroll_start_y = mouse_y() - out_handle_pos;
            }
        }

        if (is_active(id))
        {
            out_state = STATE_ACTIVE;
            out_val   = remap_range(mouse_y() - scroll_start_y, rect.y0, rect.y1 - handle_size, val_min, val_max);
        }

        out_handle_pos = remap_range(out_val, val_min, val_max, rect.y0, rect.y1 - handle_size);

        return out_state != STATE_COLD;
    }

    inline void rect(const ColorRect& rect)
    {
        rect_buffer.add(rect);
    }

    inline void rect(uint32_t color, const Rect& rect)
    {
        rect_buffer.add({ color, rect });
    }

    inline void rect(uint32_t color, float x, float y, float width, float height)
    {
        rect_buffer.add({ color, { x, y, x + width, y + height } });
    }

    inline void hline(uint32_t color, float y, float x0, float x1, float thickness = 1.0f)
    {
        // TODO : We could center it around the given `y`, but then we'd need to
        //        handle DPI here explicitly.
        rect_buffer.add({ color, { x0, y, x1, y + thickness } });
    }

    inline void vline(uint32_t color, float x, float y0, float y1, float thickness = 1.0f)
    {
        // TODO : We could center it around the given `x`, but then we'd need to
        //        handle DPI here explicitly.
        rect_buffer.add({ color, { x, y0, x + thickness, y1 } });
    }

    // Single-line text.
    inline void text_size(const char* string, float& out_width, float& out_height)
    {
        out_width  = glyph_cache.glyph_screen_width () * static_cast<float>(utf8size_lazy(string));
        out_height = glyph_cache.glyph_screen_height();
    }

    // Single-line text.
    void text(const char* string, uint32_t color, float x, float y)
    {
        text_buffer.start(color, x, y);

        utf8_int32_t codepoint = 0;

        for (void* it = utf8codepoint(string, &codepoint); codepoint; it = utf8codepoint(it, &codepoint))
        {
            // TODO : The codepoint-to-index should be handled by the glyph cache.
            if (codepoint >= 32 && codepoint <= 126)
            {
                text_buffer.add(codepoint - 32);
            }
        }

        text_buffer.end();
    }

    // Single-line text.
    void text(const char* start, const char* end, uint32_t max_chars, uint32_t color, float x, float y)
    {
        if (start != end)
        {
            text_buffer.start(color, x, y);

            utf8_int32_t codepoint = 0;
            uint32_t     i         = 0;

            for (void* it = utf8codepoint(start, &codepoint); it != end && i < max_chars; it = utf8codepoint(it, &codepoint), i++)
            {
                // TODO : The codepoint-to-index should be handled by the glyph cache.
                if (codepoint >= 32 && codepoint <= 126)
                {
                    text_buffer.add(codepoint - 32);
                }
            }

            text_buffer.end();
        }
    }
    
    bool tab(uint8_t id, const Rect& rect, const char* label)
    {
        State      state   = STATE_COLD;
        const bool clicked = button_logic(id, rect, state);

        if (state != STATE_COLD)
        {
            cursor = CURSOR_HAND;
        }

        // TODO : Centralize colors.
        constexpr uint32_t colors[] =
        {
            0xff0000ff,
            0x00ff00ff,
            0x0000ffff,
        };

        this->rect(colors[state], rect);

        float width;
        float height;
        text_size(label, width, height);

        text(label, 0xffffffff, (rect.x0 + rect.x1 - width) * 0.5f, (rect.y0 + rect.y1 - height) * 0.5f);

        return clicked;
    }

    bool vdivider(uint8_t id, float& inout_x, float y0, float y1, float thickness)
    {
        State      state  = STATE_COLD;
        float      out_y  = 0.0f;
        const bool active = drag_logic(id, { inout_x, y0, inout_x + thickness, y1 }, state, inout_x, out_y);

        if (state != STATE_COLD)
        {
            cursor = CURSOR_H_RESIZE;
        }

        // TODO : Centralize colors.
        constexpr uint32_t colors[] =
        {
            0xff0000ff,
            0x00ff00ff,
            0x0000ffff,
        };

        vline(colors[state], inout_x, y0, y1, thickness);

        return active;
    }

    bool scrollbar(uint8_t id, const Rect& rect, float& out_handle_pos,
        float handle_size, float &out_val, float val_min, float val_max)
    {
        State      state  = STATE_COLD;
        const bool active = scrollbar_logic(id, rect, state, out_handle_pos, handle_size, out_val, val_min, val_max);

        // TODO : Centralize colors.
        constexpr uint32_t colors[] =
        {
            0xff0000ff,
            0x00ff00ff,
            0x0000ffff,
        };

        this->rect(0xffffffff, rect);
        this->rect(colors[state], { rect.x0, out_handle_pos, rect.x1, out_handle_pos + handle_size });

        return active;
    }
};

static float round_to_pixel(float value)
{
    return bx::round(value * dpi()) / dpi();
}

struct Editor
{
    enum DisplayMode
    {
        RIGHT,
        LEFT,
        OVERLAY,
    };

    struct ByteRange
    {
        uint32_t start = 0;
        uint32_t end   = 0; // "One past".

        inline bool is_empty() const
        {
            return start == end;
        }

        inline bool overlaps(ByteRange other) const
        {
            // TODO ! Make sure the `<=` are correct !
            return
                (start <= other.start && end   >= other.end) ||
                (start >= other.start && start <= other.end) ||
                (end   >= other.start && end   <= other.end) ;
        }

        inline ByteRange intersect(ByteRange other) const
        {
            return { bx::max(start, other.start), bx::min(end, other.end) };
        }
    };

    struct Position
    {
        uint32_t line      = 0;
        uint32_t character = 0;
    };

    struct PositionRange
    {
        Position start;
        Position end;
    };

    Vector<char>      buffer;
    Vector<ByteRange> lines;
    ByteRange         selection;
    DisplayMode       display_mode  = RIGHT;
    float             split_x       = 0.0f;
    float             scroll_offset = 0.0f;
    int               cursor_column = 0;
    bool              cursor_at_end = false;

    void set_content(const char* string)
    {
        buffer.clear();
        lines .clear();

        lines.reserve(256);
        lines.push_back({});

        selection     = {};
        scroll_offset = 0.0f;
        cursor_column = 0;
        cursor_at_end = false;

        if (!string)
        {
            return;
        }

        utf8_int32_t codepoint = 0;
        void*        it        = nullptr;

        for (it = utf8codepoint(string, &codepoint); codepoint; it = utf8codepoint(it, &codepoint))
        {
            if (codepoint == '\n')
            {
                const uint32_t offset = static_cast<char*>(it) - string;

                lines.back().end = offset;
                lines.push_back({ offset });
            }
        }

        const uint32_t size = static_cast<char*>(it) - string - 1; // Null terminator not included.
        ASSERT(size == utf8size_lazy(string));

        if (size)
        {
            lines.back().end = size;

            buffer.reserve(size + 1024);
            buffer.resize (size);

            bx::memCopy(buffer.data(), string, size);
        }
    }

    void handle_input()
    {
        const bool up    = key_down(KEY_UP   );
        const bool down  = key_down(KEY_DOWN );
        const bool left  = key_down(KEY_LEFT );
        const bool right = key_down(KEY_RIGHT);

        if (left || right)
        {
            if (selection.is_empty())
            {
                if (left)
                {
                    while (
                        selection.start &&
                        (buffer[--selection.start] & 0b11000000) == 0b10000000);
                }

                if (right && selection.start + 1 < buffer.size())
                {
                    selection.start += utf8codepointcalcsize(&buffer[selection.start]);
                }

                selection.end = selection.start;
                cursor_column = get_position(selection.start).character;
            }
            else
            {
                selection.start = 
                selection.end   = (left ? selection.start : selection.end);
            }
        }
    }

    void update(Context& ctx, uint8_t id)
    {
        constexpr float caret_width       =  2.0f;
        constexpr float divider_thickness =  4.0f;
        constexpr float scrollbar_width   = 10.0f;
        constexpr float scrolling_speed   = 10.0f; // TODO : Is this cross-platform stable ?
        constexpr float min_handle_size   = 20.0f;

        ctx.push_id(id);

        if (split_x == 0.0f)
        {
            split_x = width() * 0.5f;
        }

        if (display_mode != OVERLAY)
        {
            ctx.vdivider(ID, split_x, 0.0f, height(), divider_thickness);
        }

        split_x = round_to_pixel(split_x);

        ColorRect viewport;
        switch (display_mode)
        {
        case RIGHT:
            viewport.rect  = { split_x + divider_thickness, 0.0f, width(), height() };
            viewport.color = 0x000000ff;
            break;
        case LEFT:
            viewport.rect  = { 0.0f, 0.0f, split_x, height()};
            viewport.color = 0x000000ff;
            break;
        case OVERLAY:
            viewport.rect  = { 0.0f, 0.0f, width(), height() };
            viewport.color = 0x000000aa;
            break;
        }

        ctx.rect(viewport);

        const float char_width  = ctx.glyph_cache.glyph_screen_width ();
        const float line_height = ctx.glyph_cache.glyph_screen_height();
        float       max_scroll  = 0.0f;

        if (lines.size() > 1)
        {
            max_scroll = line_height * bx::max(0.0f, static_cast<float>(lines.size()) - 1.0f);

            static float handle_pos  = 0.0f;
            const float  handle_size = bx::max(viewport.rect.height() * viewport.rect.height() / (max_scroll + viewport.rect.height()), min_handle_size);

            (void)ctx.scrollbar(
                ID,
                { viewport.rect.x1 - scrollbar_width, viewport.rect.y0, viewport.rect.x1, viewport.rect.y1 },
                handle_pos,
                handle_size,
                scroll_offset,
                0.0f,
                max_scroll
            );
        }

        // TODO : Need some way broadcast the editor has focus / is active.
        handle_input();

        if (viewport.rect.is_hovered() && ctx.none_active() && scroll_y())
        {
            scroll_offset = bx::clamp(scroll_offset - scroll_y() * scrolling_speed, 0.0f, max_scroll);
        }

        scroll_offset = round_to_pixel(scroll_offset);

        const size_t first_line = static_cast<size_t>(bx::floor(scroll_offset / line_height));
        const size_t line_count = static_cast<size_t>(bx::ceil (viewport.rect.height() / line_height)) + 1;
        const size_t last_line  = bx::min(first_line + line_count, lines.size());

        char  line_number[8];
        char  line_format[8];
        float line_number_width = 0.0f;

        for (size_t i = lines.size(), j = 0; ; i /= 10, j++)
        {
            if (i == 0)
            {
                (void)bx::snprintf(line_format, sizeof(line_format), "%%%zuzu ", bx::max(j + 1, static_cast<size_t>(3)));
                (void)bx::snprintf(line_number, sizeof(line_number), line_format, static_cast<size_t>(1));

                line_number_width = char_width * static_cast<float>(bx::strLen(line_number));

                break;
            }
        }

        const uint32_t max_chars = static_cast<uint32_t>(bx::max(1.0f, bx::ceil((viewport.rect.width() - line_number_width - scrollbar_width) / char_width)));

        float y = viewport.rect.y0 - bx::mod(scroll_offset, line_height);

        // Text and line numbers submission.
        for (size_t i = first_line; i < last_line; i++, y += line_height)
        {
            (void)bx::snprintf(line_number, sizeof(line_number), line_format, i);

            ctx.text(line_number, 0xaaaaaaff, viewport.rect.x0, y);
            ctx.text(buffer.data() + lines[i].start, buffer.data() + lines[i].end, max_chars, 0xffffffff, viewport.rect.x0 + line_number_width, y);
        }

        // Selection.
        const ByteRange visible_range = { lines[first_line].start, lines[last_line].start };

        if (!selection.is_empty() && selection.overlaps(visible_range))
        {
            const ByteRange visible_selection = selection.intersect(visible_range);
            ASSERT(!visible_selection.is_empty());

            Position position = get_position(visible_selection.start, first_line);
            float    y        = viewport.rect.y0 - bx::mod(scroll_offset, line_height) + line_height * (position.line - first_line);

            for (;; y += line_height, position.line++, position.character = 0)
            {
                const uint32_t offset = bx::max(selection.start, lines[position.line].start);
                if (offset >= selection.end)
                {
                    break;
                }

                const float x0 = viewport.rect.x0 + line_number_width + position.character * char_width;
                const float x1 = x0 + char_width * utf8nlen(buffer.data() + offset, bx::min(selection.end, lines[position.line].end) - offset);

                ctx.rect(0x00ff00ff, { x0, y, x1, y + line_height });
            }
        }

        // Caret.
        if (bx::fract(static_cast<float>(elapsed())) < 0.5f)
        {
            const uint32_t caret_offset = cursor_at_end ? selection.end : selection.start;

            if (visible_range.overlaps({ caret_offset, caret_offset }))
            {
                const Position caret_position = get_position(caret_offset, first_line);

                const float x = viewport.rect.x0 + line_number_width + char_width * caret_position.character;
                const float y = viewport.rect.y0 - bx::mod(scroll_offset, line_height) + line_height * (caret_position.line - first_line);

                // TODO : Make sure the caret rectangle is aligned to framebuffer pixels.
                ctx.rect(0xff0000ff, { x - caret_width * 0.5f, y, x + caret_width * 0.5f, y + line_height });
            }
        }

        ctx.pop_id();
    }

    Position get_position(uint32_t offset, uint32_t line = 0) const
    {
        Position position = {};

        for (; line < lines.size(); line++)
        {
            if (offset >= lines[line].start && offset < lines[line].end)
            {
                position.line      = line;
                position.character = utf8nlen(buffer.data() + lines[line].start, offset - lines[line].start);
                break;
            }
        }

        return position;
    }

    inline PositionRange get_position_range(ByteRange range) const
    {
        ASSERT(range.start <= range.end);

        const Position start = get_position(range.start);
        const Position end   = get_position(range.end, start.line);

        return { start, end };
    }
};

} // namespace gui
