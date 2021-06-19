#include <mnm/mnm.h>

#define MESH_ID 1

static void setup(void)
{
    title("Wireframe Triangle Example");
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

    begin_transient(MESH_ID, PRIMITIVE_LINE_STRIP | VERTEX_COLOR);
    {
        color(0xff0000ff);
        vertex(-0.6f, -0.4f, 0.0f);

        color(0x00ff00ff);
        vertex(0.6f, -0.4f, 0.0f);

        color(0x0000ffff);
        vertex(0.0f, 0.6f, 0.0f);

        color(0xff0000ff);
        vertex(-0.6f, -0.4f, 0.0f);
    }
    end();

    mesh(MESH_ID);
}

MNM_MAIN(0, setup, draw, 0);
