#include <mnm/mnm.h>

#define TRIANGLE_ID 1

#define ATLAS_ID    1

static void setup(void)
{
    title("Font Atlas Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    begin_atlas(ATLAS_ID, ATLAS_DEFAULT, 20.0f * dpi(), load_bytes("FiraCode-Regular.ttf", 0));
    glyph_range(0x020, 0x07e);
    glyph_range(0x0a1, 0x0ff);
    glyph_range(0x100, 0x17f);
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

    // identity();
    // rotate_z((float)elapsed() * -50.0f);

    // begin_mesh(TRIANGLE_ID, MESH_TRANSIENT | VERTEX_COLOR);
    // {
    //     color(0xff0000ff);
    //     vertex(-0.6f, -0.4f, 0.0f);

    //     color(0x00ff00ff);
    //     vertex(0.6f, -0.4f, 0.0f);

    //     color(0x0000ffff);
    //     vertex(0.0f, 0.6f, 0.0f);
    // }
    // end_mesh();

    // identity();
    // mesh(TRIANGLE_ID);
}

MNM_MAIN(0, setup, draw, 0);
