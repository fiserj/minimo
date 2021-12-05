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


// -----------------------------------------------------------------------------
// STB TEXTEDIT INTERNAL API IMPLEMENTATION
// -----------------------------------------------------------------------------

static constexpr STB_TEXTEDIT_CHARTYPE STB_TEXTEDIT_NEWLINE = '\n';

struct STB_TEXTEDIT_STRING
{
    char  buffer[1 << 20] = { 0 }; // 1 MB.
    int   size            = 0;     // Bytes, not characters.
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
    obj->buffer[obj->size] = '\0';
}

static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj, int char_index, const STB_TEXTEDIT_CHARTYPE* string, int char_count)
{
    const int size = utf8_byte_size(string, char_count);

    if (obj->size + size + 1 > sizeof(obj->buffer))
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
    obj->buffer[obj->size] = '\0';

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
