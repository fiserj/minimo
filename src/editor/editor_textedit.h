#pragma once

// -----------------------------------------------------------------------------
// STB TEXTEDIT HEADER-VISIBLE DEFINITIONS
// -----------------------------------------------------------------------------

#define STB_TEXTEDIT_CHARTYPE utf8_int32_t

#include <stb_textedit.h>


// -----------------------------------------------------------------------------
// UTF-8 HELPERS
// -----------------------------------------------------------------------------

static const char* utf8_advance_n_codepoints(const char *str, int n)
{
    utf8_int32_t codepoint = 0;
    const void*  it        = utf8codepoint(str, &codepoint);

    for (; codepoint && n; it = utf8codepoint(it, &codepoint), n--)
    {
    }

    return static_cast<const char*>(it);
}

static int utf8_length_until_newline(const char* str)
{
    utf8_int32_t codepoint = 0;
    int          length    = 0;

    for (const void*  it = utf8codepoint(str, &codepoint);
        codepoint && codepoint != '\n';
        it = utf8codepoint(it, &codepoint), length++)
    {
    }

    return length;
}

static int utf8_byte_size(const STB_TEXTEDIT_CHARTYPE* str, int n)
{
    size_t size = 0;

    for (const STB_TEXTEDIT_CHARTYPE* end = str + n; str != end; str++)
    {
        size += utf8codepointsize(*str);
    }

    return static_cast<int>(size);
}

static int utf8_count_lines(const char* string)
{
    int n = 1;

    for (; *string; string++)
    {
        if (*string == '\n')
        {
            n++;
        }
    }

    return n;
}


// -----------------------------------------------------------------------------
// STB TEXTEDIT INTERNAL API IMPLEMENTATION
// -----------------------------------------------------------------------------

static constexpr STB_TEXTEDIT_CHARTYPE STB_TEXTEDIT_NEWLINE = '\n';

// TODO : Add meaningful values.
enum
{
    STB_TEXTEDIT_K_LEFT,
    STB_TEXTEDIT_K_RIGHT,
    STB_TEXTEDIT_K_UP,
    STB_TEXTEDIT_K_DOWN,
    STB_TEXTEDIT_K_PGUP,
    STB_TEXTEDIT_K_PGDOWN,
    STB_TEXTEDIT_K_LINESTART,
    STB_TEXTEDIT_K_LINEEND,
    STB_TEXTEDIT_K_TEXTSTART,
    STB_TEXTEDIT_K_TEXTEND,
    STB_TEXTEDIT_K_DELETE,
    STB_TEXTEDIT_K_BACKSPACE,
    STB_TEXTEDIT_K_UNDO,
    STB_TEXTEDIT_K_REDO,

    STB_TEXTEDIT_K_SHIFT = 1 << 10,
};

struct STB_TEXTEDIT_STRING
{
    char  buffer[1 << 20] = { 0 }; // 1 MB.
    int   size            = 0;     // Bytes, not characters. Includes final '\0'.
    float char_width      = 0.0f;  // Screen coordinates.
    float line_height     = 0.0f;  // Screen coordinates.
};

static int STB_TEXTEDIT_STRINGLEN(const STB_TEXTEDIT_STRING* obj)
{
    return static_cast<int>(utf8len(obj->buffer));
}

static void STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* out_row, const STB_TEXTEDIT_STRING* obj, int char_idx)
{
    const char* string = utf8_advance_n_codepoints(obj->buffer, char_idx);
    const int   length = utf8_length_until_newline(string);

    out_row->x0               = 0.0f;
    out_row->x1               = obj->char_width * length;
    out_row->baseline_y_delta = obj->line_height;
    out_row->ymin             = 0.0f;
    out_row->ymax             = obj->line_height;
    out_row->num_chars        = length;
}

static inline float STB_TEXTEDIT_GETWIDTH(const STB_TEXTEDIT_STRING* obj, int, int)
{
    return obj->char_width;
}

static inline STB_TEXTEDIT_CHARTYPE STB_TEXTEDIT_GETCHAR(const STB_TEXTEDIT_STRING* obj, int char_idx)
{
    return *utf8_advance_n_codepoints(obj->buffer, char_idx);
}

static void STB_TEXTEDIT_DELETECHARS(STB_TEXTEDIT_STRING* obj, int char_idx, int char_count)
{
    char*       dst = const_cast<char*>(utf8_advance_n_codepoints(obj->buffer, char_idx));
    const char* src = utf8_advance_n_codepoints(dst, char_count);
    const char* end = obj->buffer + obj->size;

    const int char_bytes = static_cast<int>(src - dst);

    for (; src != end; src++, dst++)
    {
        *dst = *src;
    }

    obj->size -= char_bytes;
}

static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj, int char_index, const STB_TEXTEDIT_CHARTYPE* string, int char_count)
{
    const int size = utf8_byte_size(string, char_count);

    if (obj->size + size > sizeof(obj->buffer))
    {
        // TODO : If we have dynamic string buffer, reallocate it here.
        return false;
    }

    const char* src = utf8_advance_n_codepoints(obj->buffer, char_index);
    char*       dst = const_cast<char*>(src) + size;
    const char* end = obj->buffer + obj->size + size;

    for (; src != end; src++, dst++)
    {
        *dst = *src;
    }

    obj->size += size;

    return true;
}

static inline int STB_TEXTEDIT_KEYTOTEXT(int key)
{
    return key < 127 ? key : -1;
}


// -----------------------------------------------------------------------------
// STB TEXTEDIT LIBRARY IMPLEMENTATION
// -----------------------------------------------------------------------------

#define STB_TEXTEDIT_IMPLEMENTATION
#include <stb_textedit.h>


// -----------------------------------------------------------------------------
// EDITOR WIDGET
// -----------------------------------------------------------------------------

struct TextEditor
{
    enum DisplayMode
    {
        RIGHT,
        LEFT,
        OVERLAY,
    };

    STB_TEXTEDIT_STRING text;
    STB_TexteditState   state;
    double              blink_base_time           = 0.0;
    float               split_x                   = 0.0f; // Screen coordinates.
    float               scroll_offset             = 0.0f; // Lines (!).
    float               scrollbar_handle_position = 0.0f;
    int                 line_count                = 0;
    DisplayMode         display_mode              = RIGHT;

    void set_content(const char* string)
    {
        memset(this, 0, sizeof(*this));

        utf8ncpy(text.buffer, string, sizeof(text.buffer));
        text.size = static_cast<int>(utf8len(text.buffer)) + 1;

        line_count = utf8_count_lines(text.buffer);

        stb_textedit_initialize_state(&state, false);
    }

    void update(gui::Context& ctx, uint8_t id)
    {
        using namespace gui;

        constexpr float caret_width       =  2.0f;
        constexpr float divider_thickness =  4.0f;
        constexpr float scrollbar_width   = 10.0f;
        constexpr float scrolling_speed   = 10.0f; // TODO : Is this cross-platform stable ?
        constexpr float min_handle_size   = 20.0f;

        const float width  = ::width();
        const float height = ::height();
        const float dpi    = ::dpi();

        ctx.push_id(id);

        // Properties' update --------------------------------------------------
        text.char_width  = ctx.glyph_cache.glyph_screen_width ();
        text.line_height = ctx.glyph_cache.glyph_screen_height();

        // Input handling ------------------------------------------------------
        // TODO : Process keys, mouse clicks/drags, and clipboard handling.

        // Line number format --------------------------------------------------
        char  line_number[8];
        char  line_format[8];
        float line_number_width = 0.0f;

        for (int i = line_count, j = 0; ; i /= 10, j++)
        {
            if (i == 0)
            {
                (void)bx::snprintf(line_format, sizeof(line_format), "%%%ii ", bx::max(j + 1, 3));
                (void)bx::snprintf(line_number, sizeof(line_number), line_format, 1);

                line_number_width = text.char_width * bx::strLen(line_number);

                break;
            }
        }

        // Screen divider ------------------------------------------------------
        if (split_x == 0.0f)
        {
            split_x = width * 0.5f;
        }

        if (display_mode != OVERLAY)
        {
            ctx.vdivider(ID, split_x, 0.0f, height, divider_thickness);
        }

        split_x = bx::round(split_x * dpi) / dpi;

        // Viewport ------------------------------------------------------------
        Rect viewport;
        switch (display_mode)
        {
        case RIGHT:
            viewport = { split_x + divider_thickness, 0.0f, width, height };
            break;
        case LEFT:
            viewport = { 0.0f, 0.0f, split_x, height};
            break;
        case OVERLAY:
            viewport = { 0.0f, 0.0f, width, height };
            break;
        }

        ctx.rect(COLOR_BLACK, viewport);

        // Scrollbar -----------------------------------------------------------
        const float max_scroll  = bx::max(0.0f, line_count - 1.0f);

        if (line_count > 0)
        {
            const float handle_size = bx::max(viewport.height() * viewport.height() /
                (max_scroll  * text.line_height + viewport.height()), min_handle_size);

            (void)ctx.scrollbar(
                ID,
                { viewport.x1 - scrollbar_width, viewport.y0, viewport.x1, viewport.y1 },
                scrollbar_handle_position,
                handle_size,
                scroll_offset,
                0.0f,
                max_scroll
            );
        }

        if (viewport.is_hovered() && ctx.none_active())
        {
            if (scroll_y())
            {
                scroll_offset = bx::clamp(scroll_offset - scroll_y() * scrolling_speed / text.line_height, 0.0f, max_scroll);
            }

            // TODO : Exclude scrollbar and line number areas.
            if ((mouse_x() >= viewport.x0 + line_number_width) &&
                (mouse_x() <  viewport.x1 - scrollbar_width))
            {
                ctx.cursor = CURSOR_I_BEAM;
            }
        }

        scroll_offset = round_to_pixel(scroll_offset);

        // ...

        ctx.pop_id();
    }
};
