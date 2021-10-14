#include <mnm/mnm.h>

#define FONT_ID  1

#define ATLAS_ID 1

#define TEXT_ID  1

static void setup(void)
{
    title("Font Atlas Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    create_font(FONT_ID, load_bytes("FiraCode-Regular.ttf", 0));

    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X | ATLAS_ALLOW_UPDATE, FONT_ID, 12.0f * dpi());
    glyph_range(0x20, 0x7e);
    end_atlas();

    begin_text(TEXT_ID, ATLAS_ID, TEXT_H_ALIGN_CENTER | TEXT_V_ALIGN_MIDDLE);
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
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    identity();
    ortho(0.0f, pixel_width(), pixel_height(), 0.0f, 1.0f, -1.0f);
    // ortho(0.0f, pixel_width(), 0.0f, pixel_height(), 1.0f, -1.0f);
    projection();

    identity();
    begin_mesh(2, MESH_TRANSIENT | PRIMITIVE_LINES | VERTEX_COLOR);
    color(0x000000ff);
    vertex(pixel_width() * 0.5f, -pixel_height(), 0.0f);
    vertex(pixel_width() * 0.5f,  pixel_height(), 0.0f);
    vertex(-pixel_width(), pixel_height() * 0.5f, 0.0f);
    vertex( pixel_width(), pixel_height() * 0.5f, 0.0f);
    end_mesh();
    mesh(2);

    identity();
    translate(pixel_width() * 0.5f, pixel_height() * 0.5f, 0.0f);
    mesh(TEXT_ID);
}

MNM_MAIN(0, setup, draw, 0);
