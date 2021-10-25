#pragma once

#include <assert.h>    // assert
#include <stdint.h>    // uint*_t

#include <vector>      // vector

#include <bx/bx.h>     // BX_*LIKELY, memCopy
#include <bx/math.h>   // ceil, floor, min, mod

#include <utf8.h>      // utf8*

#include <mnm/mnm.h>   // ...

struct ByteRange
{
    uint32_t start = 0;
    uint32_t end   = 0;
};

struct TextEdit
{
    std::vector<char>      buffer;
    std::vector<ByteRange> lines;
    ByteRange              selection;
    float                  scroll_offset = 0.0f;
    bool                   cursor_at_end = false;
};

struct TextEditSettings
{
    float    font_cap_height    = 8.0f;
    float    line_height_factor = 2.0f;

    uint32_t text_color         = 0xffffffff;
    uint32_t line_number_color  = 0xaaaaaaff;
};

static void submit_lines(const TextEdit& te, const TextEditSettings& tes, float viewport_height)
{
    const float  line_height = tes.font_cap_height * tes.line_height_factor;
    const size_t first_line  = static_cast<size_t>(bx::floor(te.scroll_offset / line_height));
    const size_t line_count  = static_cast<size_t>(bx::ceil (viewport_height  / line_height)) + 1;

    push();

    scale(1.0f / dpi());
    translate(0.0f, -bx::mod(te.scroll_offset, line_height), 0.0f);

    color(tes.text_color);

    for (size_t i = first_line, n = bx::min(first_line + line_count, te.lines.size()); i < n; i++)
    {
        const ByteRange& line = te.lines[i];

        text(te.buffer.data() + line.start, te.buffer.data() + line.end);

        translate(0.0f, line_height, 0.0f);
    }

    pop();
}

static void set_content(TextEdit& te, const char* string)
{
    te.buffer.clear();
    te.lines .clear();

    te.lines.reserve(256);
    te.lines.push_back({});

    te.selection     = {};
    te.scroll_offset = 0.0f;
    te.cursor_at_end = false;

    if (BX_UNLIKELY(!string))
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

            te.lines.back().end = offset;
            te.lines.push_back({ offset });
        }
    }

    const uint32_t size = static_cast<char*>(it) - string; // Null terminator included.
    assert(size == utf8size(string));

    if (size)
    {
        te.lines.back().end = size;

        te.buffer.reserve(size + 1024);
        te.buffer.resize (size);

        bx::memCopy(te.buffer.data(), string, size);
    }
}
