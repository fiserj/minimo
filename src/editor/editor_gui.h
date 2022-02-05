#pragma once

namespace gui
{

// -----------------------------------------------------------------------------
// LIMITS
// -----------------------------------------------------------------------------

constexpr uint32_t MAX_DRAW_LIST_SIZE     = 4096;

constexpr uint32_t MAX_COLOR_PALETTE_SIZE = 32;

constexpr uint32_t MAX_CLIP_STACK_SIZE    = 4;


// -----------------------------------------------------------------------------
// DATA TYPES AND STRUCTURES
// -----------------------------------------------------------------------------

enum State
{
    STATE_COLD,
    STATE_HOT,
    STATE_ACTIVE,
};

enum Color : uint8_t
{
    COLOR_EDITOR_TEXT,
    COLOR_EDITOR_LINE_NUMBER,

    // TODO : Replace with symbolic names.
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_BLACK,
};

struct Rect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    inline bool operator==(const Rect& other) const
    {
        return
            x0 == other.x0 &&
            y0 == other.y0 &&
            x1 == other.x1 &&
            y1 == other.y1 ;
    }

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
    float glyph_width  = 0.0f; // In pixels, includes an extra pixel of padding.
    float glyph_height = 0.0f; // In pixels, no padding.

    inline float glyph_screen_width() const
    {
        return (glyph_width  - 1.0f) / dpi();
    }

    inline float glyph_screen_height() const
    {
        return glyph_height / dpi();
    }

    uint32_t codepoint_index(int codepoint) const
    {
        if (codepoint >= 32 && codepoint <= 126)
        {
            return static_cast<uint32_t>(codepoint - 32);
        }

        // TODO : Utilize hashmap for the rest of the stored characters.
        // ...

        // Replacement character.
        return 95;
    }

    void rebuild(float cap_height, const Resources& res)
    {
        ASSERT(cap_height > 0.0f);

        begin_atlas(
            res.texture_tmp_atlas,
            ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE | ATLAS_ALLOW_UPDATE,
            res.font_atlas,
            cap_height * dpi()
        );
        glyph_range(0x0020, 0x007e); // Printable ASCII.
        glyph_range(0xfffd, 0xfffd); // Replacement character.
        end_atlas();

        text_size(res.texture_tmp_atlas, "X", 0, 1.0f, &glyph_width, &glyph_height);

        glyph_width  += 1.0f;
        glyph_height *= 2.0f;

        for (texture_size = 128; ; texture_size *= 2)
        {
            // TODO : Rounding and padding.
            glyph_cols = (int)(texture_size / glyph_width );
            const int rows = (int)(texture_size / glyph_height);

            if (glyph_cols * rows >= 96) // TODO : Check against the dynamic glyph count.
            {
                break;
            }
        }

        struct GlyphWriter
        {
            GlyphCache& cache;
            int         index = 0;

            void write_glyph_range(uint32_t start, uint32_t end)
            {
                char buffer[5] = { 0 };

                for (uint32_t codepoint = start; codepoint <= end; codepoint++)
                {
                    const int x = index % cache.glyph_cols;
                    const int y = index / cache.glyph_cols;

                    index++;

                    identity();
                    translate(x * cache.glyph_width, (y + 0.25f) * cache.glyph_height, 0.0f);

                    const uint32_t size = mnm::utf8_encode(codepoint, buffer);
                    text(buffer, buffer + size);
                }
            }
        };

        begin_text(
            res.mesh_tmp_text,
            res.texture_tmp_atlas,
            TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT
        );
        {
            color(0xffffffff);

            GlyphWriter writer = { *this };

            writer.write_glyph_range(0x0020, 0x007e);
            ASSERT(writer.index == 95);

            writer.write_glyph_range(0xfffd, 0xfffd);
            ASSERT(writer.index == 96);
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

struct AtlasInfo
{
    float texel_size;
    float glyph_cols;
    float glyph_texel_width;
    float glyph_texel_height;
    float glyph_texel_to_screen_width_ratio;
    float glyph_texel_to_screen_hegiht_ratio;

    float _unused[2];
};

static_assert(sizeof(AtlasInfo) % (4 * sizeof(float)) == 0, "Invalid `AtlasInfo` size assumption.");

struct Colors
{
    float colors[MAX_COLOR_PALETTE_SIZE][4] = {};

    void set(Color color, uint32_t rgba)
    {
        ASSERT(color < MAX_COLOR_PALETTE_SIZE);

        colors[color][0] = (rgba & 0xff000000) / 255.0f;
        colors[color][1] = (rgba & 0x00ff0000) / 255.0f;
        colors[color][2] = (rgba & 0x0000ff00) / 255.0f;
        colors[color][3] = (rgba & 0x000000ff) / 255.0f;
    }
};

static_assert(sizeof(Colors) % (4 * sizeof(float)) == 0, "Invalid `Colors` size assumption.");

// Very limited clip stack. Can only host `MAX_CLIP_STACK_SIZE` unique values
// after being reset.
struct ClipStack
{
    Rect    rects[MAX_CLIP_STACK_SIZE]; // Unique values, not in LIFO order!
    uint8_t stack[MAX_CLIP_STACK_SIZE];
    uint8_t size = 0;
    uint8_t used = 0;

    void reset(const Rect& viewport)
    {
        rects[0] = viewport;
        stack[0] = 0;
        size     = 1;
        used     = 1;
    }

    uint8_t push(const Rect& rect)
    {
        ASSERT(size < MAX_CLIP_STACK_SIZE);

        uint8_t idx = UINT8_MAX;

        for (uint8_t i = 0; i < used; i++)
        {
            if (rects[i] == rect)
            {
                idx = i;
                break;
            }
        }

        ASSERT(idx != UINT8_MAX || used < MAX_CLIP_STACK_SIZE);

        if (idx == UINT8_MAX && used < MAX_CLIP_STACK_SIZE)
        {
            idx = used++;
            rects[idx] = rect;
        }

        stack[size++] = idx;

        return idx;
    }

    inline void pop()
    {
        ASSERT(size > 0);
        size--;
    }

    uint8_t top() const
    {
        ASSERT(size > 0);
        return stack[size - 1];
    }
};

struct Uniforms
{
    AtlasInfo atlas_info;
    Colors    colors;
    ClipStack clip_stack; // NOTE : Must be last.

    static constexpr int COUNT =
        (sizeof(AtlasInfo) + sizeof(Colors) + sizeof(ClipStack::rects)) / (sizeof(float) * 4);
};

// NOTE : This is to ensure that we can safely copy instance of `Uniforms`
//        object into shader without shuffling with the layout in any way.
static_assert(offsetof(Uniforms, clip_stack) + offsetof(ClipStack, stack) == 
    sizeof(AtlasInfo) + sizeof(Colors) + sizeof(ClipStack::rects),
    "Invalid assumption about `Uniforms` memory layout."
);

// Simple draw list, supports only rectangles.
struct DrawList
{
    struct Header
    {
        uint16_t glyph_count = 0;
        uint8_t  color_index = 0;
        uint8_t  clip_index  = 0;
    };

    struct Item
    {
        Header   header;
        uint32_t u32;
        float    f32;
    };

    Item     data[MAX_DRAW_LIST_SIZE]; // TODO : Dynamic memory ?
    uint32_t size              = 0;
    uint32_t offset            = 0;
    uint32_t empty_glyph_index = 0; // TODO : Set this when space not first in the atlas.

    static inline float encode_base_vertex(uint32_t glyph_index, uint8_t color_index, uint8_t clip_index)
    {
        return (((glyph_index * MAX_COLOR_PALETTE_SIZE) + color_index) * MAX_CLIP_STACK_SIZE + clip_index) * 4;
    }

    inline void clear()
    {
        size = 0;
    }

    void add_rect(const Rect& rect, uint8_t color_index, uint8_t clip_index)
    {
        ASSERT(size + 5 < MAX_DRAW_LIST_SIZE);

        data[size++].header = { 0, color_index, clip_index };
        data[size++].f32    = rect.x0;
        data[size++].f32    = rect.y0;
        data[size++].f32    = rect.x1;
        data[size++].f32    = rect.y1;
    }

    void start_string(float x, float y, uint8_t color_index, uint8_t clip_index)
    {
        ASSERT(size + 3 < MAX_DRAW_LIST_SIZE);

        offset = size;

        data[size++].header = { 0, color_index, clip_index };
        data[size++].f32    = x;
        data[size++].f32    = y;
    }

    inline void add_glyph(uint32_t index)
    {
        ASSERT(size < MAX_DRAW_LIST_SIZE);

        data[size++].u32 = index;
    }

    inline void end_string()
    {
        const uint32_t glyph_count = size - offset - 3;
        ASSERT(glyph_count <= UINT16_MAX);

        if (glyph_count)
        {
            data[offset].header.glyph_count = static_cast<uint16_t>(glyph_count);
        }
        else
        {
            // NOTE : Empty strings (glyphs not in the cache).
            size -= 3;
        }
    }

    void submit(const GlyphCache& gc, const Resources& res, Uniforms& uniforms)
    {
        if (size == 0)
        {
            return;
        }

        ASSERT(size >= 4);

        begin_mesh(res.mesh_gui_text, MESH_TRANSIENT | PRIMITIVE_QUADS | NO_VERTEX_TRANSFORM);

        const float width  = gc.glyph_screen_width ();
        const float height = gc.glyph_screen_height();

        for (uint32_t i = 0; i < size;)
        {
            const Header header = data[i++].header;

            if (header.glyph_count)
            {
                float        x0 = data[i++].f32;
                const float  y0 = data[i++].f32;
                float        x1 = x0 + width;
                const float  y1 = y0 + height;

                for (uint32_t j = 0; j < header.glyph_count; j++, i++)
                {
                    const float vtx = encode_base_vertex(data[i].u32, header.color_index, header.clip_index);

                    vertex(x0, y0, vtx + 0.0f);
                    vertex(x0, y1, vtx + 1.0f);
                    vertex(x1, y1, vtx + 2.0f);
                    vertex(x1, y0, vtx + 3.0f);

                    x0  = x1;
                    x1 += width;
                }
            }
            else
            {
                const float x0  = data[i++].f32;
                const float y0  = data[i++].f32;
                const float x1  = data[i++].f32;
                const float y1  = data[i++].f32;

                const float vtx = encode_base_vertex(empty_glyph_index, header.color_index, header.clip_index);

                vertex(x0, y0, vtx + 0.0f);
                vertex(x0, y1, vtx + 1.0f);
                vertex(x1, y1, vtx + 2.0f);
                vertex(x1, y0, vtx + 3.0f);
            }
        }

        end_mesh();

        uniforms.atlas_info =
        {
            1.0f / gc.texture_size, // Texel size.
            (float)gc.glyph_cols,
            gc.glyph_width / gc.texture_size, // Glyph size in texels, width includes padding.
            gc.glyph_height / gc.texture_size,
            gc.glyph_width / (gc.texture_size * width), // Glyph texel to screen size ratio.
            gc.glyph_height / (gc.texture_size * height),
        };

        identity();
        state   (STATE_BLEND_ALPHA | STATE_WRITE_RGB);
        uniform (res.uniform_text_info, &uniforms);
        texture (res.texture_glyph_cache);
        shader  (res.program_gui_text);
        mesh    (res.mesh_gui_text);

        offset = 0;
    }
};

// TODO : Remove `mnm::` prefixes when the whole thing is moved to the same namespace.
struct Context
{
    Resources   resources;
    IdStack     active_stack;
    IdStack     current_stack;
    DrawList    draw_list;
    GlyphCache  glyph_cache;
    Uniforms    uniforms;
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

        uniforms.clip_stack.reset({ 0.0f, 0.0f, width(), height() });
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

        draw_list.submit(glyph_cache, resources, uniforms);
        draw_list.clear ();
    }

    inline void push_id(uint8_t id)
    {
        current_stack.push(id);
    }

    inline void pop_id()
    {
        current_stack.pop();
    }

    inline void push_clip(const Rect& rect)
    {
        uniforms.clip_stack.push(rect);
    }

    inline void pop_clip()
    {
        uniforms.clip_stack.pop();
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

    inline void rect(Color color, const Rect& rect)
    {
        draw_list.add_rect(rect, color, uniforms.clip_stack.top());
    }

    inline void rect(Color color, float x, float y, float width, float height)
    {
        draw_list.add_rect({ x, y, x + width, y + height }, color, uniforms.clip_stack.top());
    }

    inline void hline(Color color, float y, float x0, float x1, float thickness = 1.0f)
    {
        // TODO : We could center it around the given `y`, but then we'd need to
        //        handle DPI here explicitly.
        draw_list.add_rect({ x0, y, x1, y + thickness }, color, uniforms.clip_stack.top());
    }

    inline void vline(Color color, float x, float y0, float y1, float thickness = 1.0f)
    {
        // TODO : We could center it around the given `x`, but then we'd need to
        //        handle DPI here explicitly.
        draw_list.add_rect({ x, y0, x + thickness, y1 }, color, uniforms.clip_stack.top());
    }

    // Single-line text.
    inline void text_size(const char* string, float& out_width, float& out_height)
    {
        out_width  = glyph_cache.glyph_screen_width () * static_cast<float>(mnm::utf8_size(string));
        out_height = glyph_cache.glyph_screen_height();
    }

    // Single-line text.
    void text(const char* string, Color color, float x, float y)
    {
        draw_list.start_string(x, y, color, uniforms.clip_stack.top());

        uint32_t codepoint;

        while ((codepoint = mnm::utf8_next_codepoint(string)))
        {
            draw_list.add_glyph(glyph_cache.codepoint_index(codepoint));
        }

        draw_list.end_string();
    }

    // Single-line text.
    void text(const char* start, const char* end, uint32_t max_chars, Color color, float x, float y)
    {
        if (start != end)
        {
            draw_list.start_string(x, y, color, uniforms.clip_stack.top());

            uint32_t codepoint;

            while ((codepoint = mnm::utf8_next_codepoint(start)) && start != end && max_chars--)
            {
                draw_list.add_glyph(glyph_cache.codepoint_index(codepoint));
            }

            draw_list.end_string();
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

        constexpr Color colors[] =
        {
            COLOR_RED,
            COLOR_GREEN,
            COLOR_BLUE,
        };

        this->rect(colors[state], rect);

        float width;
        float height;
        text_size(label, width, height);

        text(label, COLOR_EDITOR_TEXT, (rect.x0 + rect.x1 - width) * 0.5f, (rect.y0 + rect.y1 - height) * 0.5f);

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

        constexpr Color colors[] =
        {
            COLOR_RED,
            COLOR_GREEN,
            COLOR_BLUE,
        };

        vline(colors[state], inout_x, y0, y1, thickness);

        return active;
    }

    bool scrollbar(uint8_t id, const Rect& rect, float& out_handle_pos,
        float handle_size, float &out_val, float val_min, float val_max)
    {
        State      state  = STATE_COLD;
        const bool active = scrollbar_logic(id, rect, state, out_handle_pos, handle_size, out_val, val_min, val_max);

        constexpr Color colors[] =
        {
            COLOR_RED,
            COLOR_GREEN,
            COLOR_BLUE,
        };

        this->rect(COLOR_EDITOR_TEXT, rect);
        this->rect(colors[state], { rect.x0, out_handle_pos, rect.x1, out_handle_pos + handle_size });

        return active;
    }
};

static inline float round_to_pixel(float value, float dpi)
{
    return bx::round(value * dpi) / dpi;
}

static inline float round_to_pixel(float value)
{
    return round_to_pixel(value, dpi());
}

} // namespace gui
