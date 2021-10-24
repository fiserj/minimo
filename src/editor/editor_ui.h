#pragma once

#include <assert.h>    // assert
#include <stdint.h>    // uint*_t

#include <vector>      // vector

#include <bgfx/bgfx.h> // bgfx::*

#include <utf8.h>      // utf8*

#include <mnm/mnm.h>   // ...

class Editor
{
public:
    void set_content(const char* string)
    {
        m_buffer.clear();
        m_lines .clear();

        m_lines.reserve(256);
        m_lines.push_back({});

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

                m_lines.back().end = offset;
                m_lines.push_back({ offset });
            }
        }

        const uint32_t size = static_cast<char*>(it) - string; // Null terminator included.
        assert(size == utf8size(string));

        if (size)
        {
            m_lines.back().end = size;

            m_buffer.reserve(size + 1024);
            m_buffer.resize (size);

            bx::memCopy(m_buffer.data(), string, size);
        }
    }

    void set_viewport(float x, float y, float width, float height)
    {
        m_viewport = { x, y, width, height };
    }

    void update()
    {
        // ...
    }

private:
    struct ByteRange
    {
        uint32_t start = 0;
        uint32_t end   = 0;
    };

    struct Viewport
    {
        float x;
        float y;
        float width;
        float height;
    };

private:
    std::vector<char>      m_buffer;
    std::vector<ByteRange> m_lines;
    Viewport               m_viewport;
    ByteRange              m_selection;
    uint32_t               m_cursor = 0;
};