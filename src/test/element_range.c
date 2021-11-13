#include <mnm/mnm.h>

#define SQUARES_ID 1

static void setup(void)
{
    title("Element Range Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    begin_mesh(SQUARES_ID, PRIMITIVE_QUADS | VERTEX_COLOR);
    {
        identity();
        translate(-0.75f, 0.0f, 0.0f);

        const unsigned int colors[] =
        {
            0xff0000ff,
            0x00ff00ff,
            0x0000ffff,
        };

        for (int i = 0; i < 3; i++)
        {
            color(colors[i]);

            vertex(-0.2f,  0.2f, 0.0f);
            vertex(-0.2f, -0.2f, 0.0f);
            vertex( 0.2f, -0.2f, 0.0f);
            vertex( 0.2f,  0.2f, 0.0f);

            translate(0.75f, 0.0f, 0.0f);
        }
    }
    end_mesh();
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
    range(((int)elapsed() % 3) * 4, 4);
    mesh(SQUARES_ID);
}

MNM_MAIN(0, setup, draw, 0);
