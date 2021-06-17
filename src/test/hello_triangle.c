#include <mnm/mnm.h>

#define MESH_ID 1

static void setup(void)
{
    title("Hello Triangle Example");
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    projection();
    identity();
    ortho(-aspect(), aspect(), -1.0f, 1.0f, 1.0f, -1.0f);

    model();
    identity();
    rotate_z((float)elapsed() * -50.0f);

    begin_transient(MESH_ID, VERTEX_COLOR);
    {
        color(0xff0000ff);
        vertex(-0.6f, -0.4f, 0.0f);

        color(0x00ff00ff);
        vertex(0.6f, -0.4f, 0.0f);

        color(0x0000ffff);
        vertex(0.f, 0.6f, 0.0f);
    }
    end();

    mesh(MESH_ID);
}

MNM_MAIN(setup, draw, 0);
