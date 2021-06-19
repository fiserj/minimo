#include <mnm/mnm.h>

#define MESH_ID    1

#define TEXTURE_ID 1

static void setup(void)
{
    title("Hello Triangle Example");

    const unsigned int abgr[] =
    {
        0xff0000ff, 0xff00ff00,
        0xffff0000, 0xffffffff,
    };

    load_texture(TEXTURE_ID, TEXTURE_NEAREST | TEXTURE_CLAMP, 2, 2, 0, abgr);
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

    begin_transient(MESH_ID, VERTEX_TEXCOORD);
    {
        texcoord(0.0f, 1.0f);
        vertex(-0.6f, -0.4f, 0.0f);

        texcoord(1.0f, 1.0f);
        vertex(0.6f, -0.4f, 0.0f);

        texcoord(0.5f, 0.0f);
        vertex(0.0f, 0.6f, 0.0f);
    }
    end();

    texture(TEXTURE_ID);
    mesh(MESH_ID);
}

MNM_MAIN(0, setup, draw, 0);
