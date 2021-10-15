#include <mnm/mnm.h>

#define FONT_ID  1

#define ATLAS_ID 1

#define TEXT_ID  1

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

    begin_text(TEXT_ID, ATLAS_ID, TEXT_TRANSIENT | TEXT_H_ALIGN_LEFT | TEXT_V_ALIGN_CAP_HEIGHT);
    {
        color(0xffffffff);
        text
        (
            "Put in in a deck for our standup today lose client to 10:00\n"
            "meeting big picture, nor screw the pooch move the needle, so\n"
            "enough to wash your face for we need to get all stakeholders up\n"
            "to speed and in the right place. Are we in agreeance\n"
            "incentivization so blue money, but regroup yet good optics\n"
            "anti-pattern. Increase the pipelines. The last person we talked\n"
            "to said this would be ready single wringable neck or usabiltiy.\n"
            "Our competitors are jumping the shark. Re-inventing the wheel\n"
            "can you slack it to me? Innovation is hot right now optics but\n"
            "due diligence quantity."
        );
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
