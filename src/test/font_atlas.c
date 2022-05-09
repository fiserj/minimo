#include <mnm/mnm.h>

#define FONT_ID  1

#define ATLAS_ID 1

#define TEXT_ID  1
#define AXES_ID  32

#include "fira_code_regular.h"

static void setup(void)
{
    title("Font Atlas Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    create_font(FONT_ID, fira_code_regular);

    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X | ATLAS_ALLOW_UPDATE, FONT_ID, 12.0f * dpi());
    glyph_range(0x20, 0x7e);
    end_atlas();

    const int h_align[] =
    {
        TEXT_H_ALIGN_LEFT,
        TEXT_H_ALIGN_CENTER,
        TEXT_H_ALIGN_RIGHT,
    };

    const int v_align[] =
    {
        TEXT_V_ALIGN_BASELINE,
        TEXT_V_ALIGN_MIDDLE,
        TEXT_V_ALIGN_CAP_HEIGHT,
    };

    for (int v = 0, i = 0; v < 3; v++     )
    for (int h = 0       ; h < 3; h++, i++)
    {
        begin_text(TEXT_ID + i, ATLAS_ID, h_align[h] | v_align[v]);
        {
            color(0xffffffff);
            text
            (
                "Put in on a deck for our standup\n"
                "today lose client to 10:00 meeting\n"
                "big picture, nor screw the pooch\n"
                "move the needle, so enough to wash\n"
                "your face for we need to get all\n"
                "stakeholders up to speed and in\n"
                "the right place.",
                0
            );
        }
        end_text();
    }
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    identity();
    ortho(0.0f, pixel_width(), pixel_height(), 0.0f, 1.0f, -1.0f);
    projection();

    identity();
    begin_mesh(AXES_ID, MESH_TRANSIENT | PRIMITIVE_LINES | VERTEX_COLOR);
    {
        color (0xff0000ff);
        vertex(pixel_width() * 0.5f, -pixel_height(), 0.0f);
        vertex(pixel_width() * 0.5f,  pixel_height(), 0.0f);

        color (0x00ff00ff);
        vertex(-pixel_width(), pixel_height() * 0.5f, 0.0f);
        vertex( pixel_width(), pixel_height() * 0.5f, 0.0f);
    }
    end_mesh();
    mesh(AXES_ID);

    identity();
    translate(pixel_width() * 0.5f, pixel_height() * 0.5f, 0.0f);

    const int offset = (int)(elapsed() * 0.5) % 9;
    mesh(TEXT_ID + offset);
}

MNM_MAIN(0, setup, draw, 0);
