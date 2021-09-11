#include <mnm/mnm.h>

#define FONT_ID     1

#define ATLAS_ID    1

static void setup(void)
{
    title("Font Atlas Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    create_font(FONT_ID, load_bytes("FiraCode-Regular.ttf", 0));

    begin_atlas(ATLAS_ID, ATLAS_DEFAULT);
    {
        font(FONT_ID);
        font_size(20.0f * dpi());

        glyph_range(0x020, 0x07e);
        glyph_range(0x0a1, 0x0ff);
        glyph_range(0x100, 0x17f);
    }
    end_atlas();
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    identity();
    ortho(-aspect(), aspect(), -1.0f, 1.0f, 1.0f, -1.0f);
    projection();

    // ...
}

MNM_MAIN(0, setup, draw, 0);
