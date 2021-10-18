// TODO (mostly for the core library):
// * Replace stdlib-based memory managements with something managed by MiNiMo.
// * Add public UTF-8 helpers.
// * Better error handling.
// * Cache line lengths.

#include <mnm/mnm.h>

#include <assert.h> // assert
#include <math.h>   // roundf
#include <stdlib.h> // calloc, free
#include <stdint.h> // uint*_t
#include <stdio.h>  // snprintf
#include <string.h> // memcpy

#define MAX_LINE_BYTES 255

#define FONT_ID        1

#define ATLAS_ID       1

#define TEXT_ID        1

enum
{
    UTF8_ACCEPT,
    UTF8_REJECT,
};

static constexpr unsigned s_utf8_data[] =
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

static inline uint32_t utf8_decode(uint32_t* state, uint32_t* codepoint, uint32_t byte)
{
    uint32_t type = s_utf8_data[byte];

    *codepoint = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codepoint << 6) : (0xff >> type) & (byte);

    *state = s_utf8_data[256 + *state + type];

    return *state;
}

struct Settings
{
    uint32_t text_color  = 0xffffffff; // RGBA/
    float    font_size   = 10.0f;      // Cap height.
    float    line_height = 2.0f;       // Multiple of `font_size`.
};

static Settings g_settings;

struct Line
{
    uint8_t length;
    char    bytes[MAX_LINE_BYTES];
};

struct Editor
{
    // Edited file loaded in a grid.
    Line* lines          = nullptr;
    int   line_count     = 0;
    int   max_line_count = 0;

    // Viewport (in screen coordinates).
    float viewport_x0    = 0.0f;
    float viewport_y0    = 0.0f;
    float viewport_x1    = 0.0f;
    float viewport_y1    = 0.0f;

    // Cursor.
    int   cursor_col     = 0;
    int   cursor_row     = 0;
    bool  cursor_at_end  = false;

    inline bool mouse_over_viewport() const
    {
        return
            mouse_x() >= viewport_x0 && mouse_x() <= viewport_x1 &&
            mouse_y() >= viewport_y0 && mouse_y() <= viewport_y1;
    }

    void unload_content()
    {
        if (lines)
        {
            free(lines);

            lines          = nullptr;
            line_count     = 0;
            max_line_count = 0;
        }
    }

    void load_file(const char* file_name, int extra_line_count = 32)
    {
        unload_content();

        if (char* string = load_string(file_name))
        {
            line_count = 1;

            uint32_t codepoint = 0;
            uint32_t state     = 0;

            for (const char* byte = string; *byte; byte++)
            {
                if (UTF8_ACCEPT == utf8_decode(&state, &codepoint, *byte) && codepoint == '\n')
                {
                    // TODO : Increment the line count not always by one, but by
                    //        how many lines the line will be split into (if it
                    //        exceeds the line length limit (255 bytes), it will
                    //        be split into two or more lines).
                    line_count++;
                }
            }

            assert(state == UTF8_ACCEPT);

            max_line_count = line_count + extra_line_count;

            if ((lines = (Line*)calloc(max_line_count, sizeof(Line))))
            {
                Line* line = lines;
                codepoint  = 0;
                state      = 0;

                // TODO : Split lines longer than the limit (255 bytes).
                for (const char* byte = string; *byte; byte++)
                {
                    if (UTF8_ACCEPT == utf8_decode(&state, &codepoint, *byte))
                    {
                        // TODO : Convert tabs to spaces?
                        if (codepoint == '\n')
                        {
                            line++;
                            continue;
                        }
                    }

                    line->bytes[line->length++] = *byte;
                }

                assert(state == UTF8_ACCEPT);
            }

            unload(string);
        }
    }

    void insert_new_lines(int line_index, int count)
    {
        // TODO
    }

    void update_mesh() const
    {
        const float line_offset = roundf(g_settings.font_size * g_settings.line_height * dpi());

        push();

        color(g_settings.text_color);

        // TODO : Only submit visible lines.
        for (int i = 0; i < line_count; i++)
        {
            if (lines[i].length > 0)
            {
                text(lines[i].bytes, lines[i].bytes + lines[i].length);
            }

            // TODO : Check that error doesn't accumulate.
            translate(0.0f, line_offset, 0.0f);
        }

        pop();
    }
};

static Editor g_editor;

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x101010ff);
    clear_depth(1.0f);

    create_font(FONT_ID, load_bytes("FiraCode-Regular.ttf", 0));

    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X, FONT_ID, g_settings.font_size * dpi());
    glyph_range(0x20, 0x7e);
    end_atlas();

    g_editor.load_file("../src/test/static_geometry.c");
}

static void cleanup()
{
    g_editor.unload_content();
}

static void draw()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    begin_text(TEXT_ID, ATLAS_ID, TEXT_TRANSIENT);
    g_editor.update_mesh();
    end_text();

    identity();
    ortho(0.0f, pixel_width(), pixel_height(), 0.0f, 1.0f, -1.0f);
    projection();

    identity();
    translate(dpi() * 10.0f, dpi() * 15.0f, 0.0f);
    mesh(TEXT_ID);
}

MNM_MAIN(nullptr, setup, draw, cleanup);
