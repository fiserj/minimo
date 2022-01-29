#pragma once

#ifndef UINT32_MAX
#   error "'<stdint.h>' has to be included before including 'mnm_utf8.h'."
#endif

#ifndef MNM_UTF8_INCLUDED
#   define MNM_UTF8_INCLUDED
#else
#   error "Please don't include 'mnm_utf8.h' header repeatedly.
#endif

#ifndef ASSERT
#   include <assert.h>
#   define ASSERT(cond) assert(cond)
#endif

namespace mnm
{

enum : uint32_t
{
    UTF8_ACCEPT = 0,
    UTF8_REJECT = 12,
};

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
static constexpr uint8_t s_utf8_decoder_table[] =
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
static inline uint32_t utf8_decode(uint32_t& inout_state, uint32_t byte)
{
    const uint32_t type = s_utf8_decoder_table[byte];

    inout_state = s_utf8_decoder_table[256 + inout_state + type];

    return inout_state;
}

// Decodes next `byte`, transitions the decoder state, and accumulates the codepoint.
static inline uint32_t utf8_decode(uint32_t& inout_state, uint32_t byte, uint32_t& out_codepoint)
{
    const uint32_t type = s_utf8_decoder_table[byte];

    out_codepoint = (inout_state != UTF8_ACCEPT)
        ? (byte & 0x3fu) | (out_codepoint << 6)
        : (0xff >> type) & (byte);

    inout_state = s_utf8_decoder_table[256 + inout_state + type];

    return inout_state;
}

// Encodes the `codepoint` bytes into `out_string` buffer and returns its size.
static uint32_t utf8_encode(uint32_t codepoint, char* out_string)
{
    ASSERT(out_string);

    if (0 == (uint32_t{0xffffff80} & codepoint))
    {
        out_string[0] = char{codepoint};

        return 1;
    }
    else if (0 == (uint32_t{0xfffff800} & codepoint))
    {
        out_string[0] = char{0xc0 | ((codepoint >> 6) & 0x1f)};
        out_string[1] = char{0x80 | ((codepoint     ) & 0x3f)};

        return 2;
    }
    else if (0 == (uint32_t{0xffff0000} & codepoint))
    {
        out_string[0] = char{0xe0 | ((codepoint >> 12) & 0x0f)};
        out_string[1] = char{0x80 | ((codepoint >>  6) & 0x3f)};
        out_string[2] = char{0x80 | ((codepoint      ) & 0x3f)};

        return 3;
    }
    else if (0 == (uint32_t{0xffe00000} & codepoint))
    {
        out_string[0] = char{0xf0 | ((codepoint >> 18) & 0x07)};
        out_string[1] = char{0x80 | ((codepoint >> 12) & 0x3f)};
        out_string[2] = char{0x80 | ((codepoint >>  6) & 0x3f)};
        out_string[3] = char{0x80 | ((codepoint      ) & 0x3f)};

        return 4;
    }

    ASSERT(false && "Invalid codepoint.");

    return 0;
}

// Number of codepoints in `string`, looking at most at first `max_bytes` bytes.
static uint32_t utf8_length(const char* string, uint32_t max_bytes = UINT32_MAX)
{
    ASSERT(string);

    uint32_t state = UTF8_ACCEPT;
    uint32_t count = 0;

    for (; *string && max_bytes; string++, max_bytes--)
    {
        if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const uint8_t*>(string)))
        {
            count ++;
        }
    }

    ASSERT(state == UTF8_ACCEPT);

    return count;
}

// Number of bytes in `string`, looking at most at first `max_bytes` bytes.
// Excludes the `NULL` terminator.
static uint32_t utf8_size(const char* string, uint32_t max_bytes = UINT32_MAX)
{
    ASSERT(string);

    uint32_t size = 0;

    for (; *string && size < max_bytes; string++, size++);

    return size;
}

// Size of the first codepoint in `string`.
static uint32_t utf8_codepoint_size(const char* string)
{
    ASSERT(string);

    uint32_t state = UTF8_ACCEPT;
    uint32_t size  = 0;

    for (; *string; string++, size++)
    {
        if (UTF8_ACCEPT == utf8_decode(state, *reinterpret_cast<const uint8_t*>(string)))
        {
            break;
        }
    }

    ASSERT(state == UTF8_ACCEPT);
    ASSERT(size > 0 && size < 5);

    return size;
}

// Reads next codepoint from `string` and advances the pointer accordingly.
inline uint32_t utf8_next_codepoint(const char*& string)
{
    ASSERT(string);

    uint32_t codepoint;
    uint32_t state = UTF8_ACCEPT;

    for (; *string; string++)
    {
        if (UTF8_ACCEPT == utf8_decode(
            state,
            *reinterpret_cast<const uint8_t*>(string),
            codepoint
        ))
        {
            break;
        }
    }

    ASSERT(state == UTF8_ACCEPT);

    return codepoint;
}

// Reads previous codepoint from `string` and advances the pointer accordingly.
inline uint32_t utf8_next_codepoint(const char*& string)
{
    ASSERT(string);

    do
    {
        string--;
    }
    while ((0 != (0x80 & string[0])) && (0x80 == (0xc0 & string[0])));

    const char* copy = string;

    return utf8_next_codepoint(copy);
}

} // namespace mnm
