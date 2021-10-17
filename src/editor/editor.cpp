#include <mnm/mnm.h>

#define FONT_ID  1

#define ATLAS_ID 1

#define TEXT_ID  1

struct Settings
{
    float font_size = 10.0f; // Cap height.
};

struct Editor
{
    // Edited file loaded in a grid.
    char*                text           = nullptr;
    int                  line_count     = 0;
    int                  max_line_count = 0;

    // Viewport (in screen coordinates).
    float                viewport_x0    = 0.0f;
    float                viewport_y0    = 0.0f;
    float                viewport_x1    = 0.0f;
    float                viewport_y1    = 0.0f;

    // Cursor.
    int                  cursor_col     = 0;
    int                  cursor_row     = 0;
    bool                 cursor_at_end  = false;

    // Grid limits.
    static constexpr int MAX_LINE_BYTES = 256;

    inline bool mouse_over_viewport() const
    {
        return
            mouse_x() >= viewport_x0 && mouse_x() <= viewport_x1 &&
            mouse_y() >= viewport_y0 && mouse_y() <= viewport_y1;
    }

    void load_file(const char* file_name, int extra_line_count = 32)
    {
        // TODO
    }

    void insert_new_lines(int line_index, int count)
    {
        // TODO
    }
};

static Settings g_settings;

static const char* g_text = nullptr;

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x101010ff);
    clear_depth(1.0f);

    create_font(FONT_ID, load_bytes("FiraCode-Regular.ttf", 0));

    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X, FONT_ID, g_settings.font_size * dpi());
    glyph_range(0x20, 0x7e);
    end_atlas();

    g_text = load_string("../src/test/static_geometry.c");
}

static void draw()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    begin_text(TEXT_ID, ATLAS_ID, TEXT_TRANSIENT | TEXT_H_ALIGN_LEFT | TEXT_V_ALIGN_CAP_HEIGHT);
    {
        color(0xffffffff);
        text(g_text);
    }
    end_text();

    identity();
    ortho(0.0f, pixel_width(), pixel_height(), 0.0f, 1.0f, -1.0f);
    projection();

    identity();
    translate(dpi() * 10.0f, dpi() * 10.0f, 0.0f);
    mesh(TEXT_ID);
}

MNM_MAIN(nullptr, setup, draw, nullptr);
