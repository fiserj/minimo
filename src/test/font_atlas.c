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

    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X | ATLAS_ALLOW_UPDATE, FONT_ID, 30.0f * dpi());
    glyph_range('A', 'B');
    end_atlas();

    begin_text(TEXT_ID, ATLAS_ID, TEXT_H_ALIGN_CENTER | TEXT_V_ALIGN_MIDDLE | TEXT_Y_AXIS_UP);
    {
        color(0xffffffff);
        text("AB");
    }
    end_text();

    begin_text(TEXT_ID + 2, ATLAS_ID, TEXT_H_ALIGN_CENTER | TEXT_V_ALIGN_MIDDLE | TEXT_Y_AXIS_UP);
    {
        color(0xffffffff);
        text("ABCDEF");
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
    // ortho(0.0f, pixel_width(), pixel_height(), 0.0f, 1.0f, -1.0f);
    ortho(0.0f, pixel_width(), 0.0f, pixel_height(), 1.0f, -1.0f);
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
    if (((int)elapsed() / 4) % 2 == 1)
        mesh(TEXT_ID + 2);
    else 
        mesh(TEXT_ID);
}

MNM_MAIN(0, setup, draw, 0);
