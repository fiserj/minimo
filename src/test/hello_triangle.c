#include <mnm/mnm.h>

#define TRIANGLE_ID 1

static void setup(void)
{
    title("Hello Triangle Example");

    clear_color(0x333333ff);
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

    identity();
    rotate_z((float)elapsed() * -50.0f);

    begin_mesh(TRIANGLE_ID, MESH_TRANSIENT | VERTEX_COLOR);
    {
        color(0xff0000ff);
        vertex(-0.6f, -0.4f, 0.0f);

        color(0x00ff00ff);
        vertex(0.6f, -0.4f, 0.0f);

        color(0x0000ffff);
        vertex(0.0f, 0.6f, 0.0f);
    }
    end_mesh();

    mesh(TRIANGLE_ID);
}

MNM_MAIN(0, setup, draw, 0);
