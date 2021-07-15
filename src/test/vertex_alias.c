#include <mnm/mnm.h>

#define CUBE_MESH    1

#define CUBE_TEXTURE 1

static void setup(void)
{
    title("Vertex Alias Example");

    clear_color(0x101010ff);
    clear_depth(1.0f);

    const unsigned int abgr[] =
    {
        0xff404040, 0xffeeeeee,
        0xffeeeeee, 0xff404040,
    };
    load_texture(CUBE_TEXTURE, TEXTURE_NEAREST | TEXTURE_CLAMP, 2, 2, 0, abgr);

    begin_mesh(CUBE_MESH, PRIMITIVE_QUADS | VERTEX_COLOR | VERTEX_TEXCOORD);
    {
        color (0xfff200ff);
        texcoord(1.0f, 0.0f);
        vertex( 0.5f,  0.5f, -0.5f);
        texcoord(1.0f, 1.0f);
        vertex(-0.5f,  0.5f, -0.5f);
        texcoord(0.0f, 1.0f);
        vertex(-0.5f,  0.5f,  0.5f);
        texcoord(0.0f, 0.0f);
        vertex( 0.5f,  0.5f,  0.5f);

        color(0x65def1ff);
        texcoord(1.0f, 0.0f);
        vertex( 0.5f, -0.5f,  0.5f);
        texcoord(1.0f, 1.0f);
        vertex(-0.5f, -0.5f,  0.5f);
        texcoord(0.0f, 1.0f);
        vertex(-0.5f, -0.5f, -0.5f);
        texcoord(0.0f, 0.0f);
        vertex( 0.5f, -0.5f, -0.5f);

        color(0xf96900ff);
        texcoord(1.0f, 0.0f);
        vertex( 0.5f,  0.5f,  0.5f);
        texcoord(1.0f, 1.0f);
        vertex(-0.5f,  0.5f,  0.5f);
        texcoord(0.0f, 1.0f);
        vertex(-0.5f, -0.5f,  0.5f);
        texcoord(0.0f, 0.0f);
        vertex( 0.5f, -0.5f,  0.5f);

        color(0xdc2e73ff);
        texcoord(1.0f, 0.0f);
        vertex( 0.5f, -0.5f, -0.5f);
        texcoord(1.0f, 1.0f);
        vertex(-0.5f, -0.5f, -0.5f);
        texcoord(0.0f, 1.0f);
        vertex(-0.5f,  0.5f, -0.5f);
        texcoord(0.0f, 0.0f);
        vertex( 0.5f,  0.5f, -0.5f);

        color(0x5d00ffff);
        texcoord(1.0f, 0.0f);
        vertex(-0.5f,  0.5f,  0.5f);
        texcoord(1.0f, 1.0f);
        vertex(-0.5f,  0.5f, -0.5f);
        texcoord(0.0f, 1.0f);
        vertex(-0.5f, -0.5f, -0.5f);
        texcoord(0.0f, 0.0f);
        vertex(-0.5f, -0.5f,  0.5f);

        color(0x000c7dff);
        texcoord(1.0f, 0.0f);
        vertex( 0.5f,  0.5f, -0.5f);
        texcoord(1.0f, 1.0f);
        vertex( 0.5f,  0.5f,  0.5f);
        texcoord(0.0f, 1.0f);
        vertex( 0.5f, -0.5f,  0.5f);
        texcoord(0.0f, 0.0f);
        vertex( 0.5f, -0.5f, -0.5f);
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
    perspective(60.0f, aspect(), 0.1f, 100.0f);
    projection();

    identity();
    look_at(0.0f, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    view();

    identity();
    rotate_x(((float)elapsed() + 1.0f) * 57.2958f);
    rotate_y(((float)elapsed() + 2.0f) * 57.2958f);

    const int aliases[] =
    {
        VERTEX_COLOR,
        VERTEX_TEXCOORD,
        VERTEX_COLOR | VERTEX_TEXCOORD,
    };

    for (char x = -2; x <= 2; x += 2)
    {
        push();
        translate(x, 0.0f, 0.0f);

        alias(aliases[(x + 2) / 2]);
        texture(CUBE_TEXTURE);
        mesh(CUBE_MESH);

        pop();
    }
}

MNM_MAIN(0, setup, draw, 0);
