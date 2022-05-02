#pragma once

enum : u32
{
    UTF8_ACCEPT = 0,
    UTF8_REJECT = 12,
};

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
constexpr u8 s_utf8_decoder_table[] =
{
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
     0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12, 
};

// Decodes next `byte` and transitions the decoder state.
u32 utf8_decode(u32& inout_state, u32 byte)
{
    const u32 type = s_utf8_decoder_table[byte];

    inout_state = s_utf8_decoder_table[256 + inout_state + type];

    return inout_state;
}

// Decodes next `byte`, transitions the decoder state, and accumulates the codepoint.
u32 utf8_decode(u32& inout_state, u32 byte, u32& out_codepoint)
{
    const u32 type = s_utf8_decoder_table[byte];

    out_codepoint = (inout_state != UTF8_ACCEPT)
        ? (byte & 0x3fu) | (out_codepoint << 6)
        : (0xff >> type) & (byte);

    inout_state = s_utf8_decoder_table[256 + inout_state + type];

    return inout_state;
}

// Encodes the `codepoint` bytes into `out_string` buffer and returns its size.
u32 utf8_encode(u32 codepoint, char* out_string)
{
    ASSERT(out_string, "Invalid output string pointer.");

    if (0 == (UINT32_C(0xffffff80) & codepoint))
    {
        out_string[0] = char(codepoint);

        return 1;
    }
    else if (0 == (UINT32_C(0xfffff800) & codepoint))
    {
        out_string[0] = char(0xc0 | ((codepoint >> 6) & 0x1f));
        out_string[1] = char(0x80 | ((codepoint     ) & 0x3f));

        return 2;
    }
    else if (0 == (UINT32_C(0xffff0000) & codepoint))
    {
        out_string[0] = char(0xe0 | ((codepoint >> 12) & 0x0f));
        out_string[1] = char(0x80 | ((codepoint >>  6) & 0x3f));
        out_string[2] = char(0x80 | ((codepoint      ) & 0x3f));

        return 3;
    }
    else if (0 == (UINT32_C(0xffe00000) & codepoint))
    {
        out_string[0] = char(0xf0 | ((codepoint >> 18) & 0x07));
        out_string[1] = char(0x80 | ((codepoint >> 12) & 0x3f));
        out_string[2] = char(0x80 | ((codepoint >>  6) & 0x3f));
        out_string[3] = char(0x80 | ((codepoint      ) & 0x3f));

        return 4;
    }

    ASSERT(false, "Invalid codepoint.");

    return 0;
}

// Number of codepoints in `string`, looking at most at first `max_bytes` bytes.
u32 utf8_length(const char* string, u32 max_bytes = U32_MAX)
{
    ASSERT(string, "Invalid string pointer");

    u32 state = UTF8_ACCEPT;
    u32 count = 0;

    for (; *string && max_bytes; string++, max_bytes--)
    {
        if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const u8*>(string)))
        {
            count ++;
        }
    }

    ASSERT(state == UTF8_ACCEPT, "Ill-formated UTF-8 string.");

    return count;
}

// Number of bytes in `string`, looking at most at first `max_bytes` bytes.
// Excludes the `NULL` terminator.
u32 utf8_size(const char* string, u32 max_bytes = U32_MAX)
{
    ASSERT(string, "Invalid string pointer.");

    u32 size = 0;

    for (; *string && size < max_bytes; string++, size++);

    return size;
}

// Size of the first codepoint in `string`.
u32 utf8_codepoint_size(const char* string)
{
    ASSERT(string, "Invalid string pointer.");

    u32 state = UTF8_ACCEPT;
    u32 size  = 0;

    for (; *string; string++, size++)
    {
        if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const u8*>(string)))
        {
            break;
        }
    }

    ASSERT(state == UTF8_ACCEPT, "Ill-formated UTF8 string.");
    ASSERT(size > 0 && size < 5, "Invalid codepoint size %" PRIu32 ".", size);

    return size;
}

// Reads next codepoint from `string` and advances the pointer accordingly.
u32 utf8_next_codepoint(const char*& string)
{
    ASSERT(string, "Invalid string pointer.");

    u32 codepoint = 0;
    u32 state     = UTF8_ACCEPT;

    while (*string)
    {
        utf8_decode(state, *reinterpret_cast<const u8*>(string), codepoint);

        string++;

        if (UTF8_ACCEPT == state)
        {
            break;
        }
    }

    ASSERT(state == UTF8_ACCEPT, "Ill-formated UTF8 string.");

    return codepoint;
}

// Reads previous codepoint from `string` and advances the pointer accordingly.
u32 utf8_prev_codepoint(const char*& string)
{
    ASSERT(string, "Invalid string pointer.");

    do
    {
        string--;
    }
    while ((0 != (0x80 & string[0])) && (0x80 == (0xc0 & string[0])));

    const char* copy = string;

    return utf8_next_codepoint(copy);
}
