#include <mnm/mnm.h>

#define FONT_ID  1

#define ATLAS_ID 1

struct Settings
{
    float font_size = 12.0f;
};

static Settings g_settings;

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x1e1e1eff);
    clear_depth(1.0f);

    create_font(FONT_ID, load_bytes("FiraCode-Regular.ttf", 0));

    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X, FONT_ID, g_settings.font_size * dpi());
    glyph_range(0x20, 0x7e);
    end_atlas();
}

static void draw()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    // ...
}

MNM_MAIN(nullptr, setup, draw, nullptr);
