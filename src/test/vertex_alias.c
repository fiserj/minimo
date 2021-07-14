#include <mnm/mnm.h>

#define CUBE_MESH    1

#define CUBE_TEXTURE 1

static void setup(void)
{
    title("Vertex Alias Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    const unsigned int abgr[] =
    {
        // 0xff0000ff, 0xff00ff00,
        // 0xffff0000, 0xffffffff,
        0xccccccff, 0xccccccff,
        0xccccccff, 0xccccccff,
    };
    load_texture(CUBE_TEXTURE, TEXTURE_NEAREST | TEXTURE_CLAMP, 2, 2, 0, abgr);

    begin_mesh(CUBE_MESH, PRIMITIVE_QUADS | VERTEX_COLOR | VERTEX_TEXCOORD);
    {
        texcoord(0.0f, 0.0f);

        color (0xfff200ff);
        vertex( 0.5f,  0.5f, -0.5f);
        vertex(-0.5f,  0.5f, -0.5f);
        vertex(-0.5f,  0.5f,  0.5f);
        vertex( 0.5f,  0.5f,  0.5f);

        color(0x65def1ff);
        vertex( 0.5f, -0.5f,  0.5f);
        vertex(-0.5f, -0.5f,  0.5f);
        vertex(-0.5f, -0.5f, -0.5f);
        vertex( 0.5f, -0.5f, -0.5f);

        color(0xf96900ff);
        vertex( 0.5f,  0.5f,  0.5f);
        vertex(-0.5f,  0.5f,  0.5f);
        vertex(-0.5f, -0.5f,  0.5f);
        vertex( 0.5f, -0.5f,  0.5f);

        color(0xdc2e73ff);
        vertex( 0.5f, -0.5f, -0.5f);
        vertex(-0.5f, -0.5f, -0.5f);
        vertex(-0.5f,  0.5f, -0.5f);
        vertex( 0.5f,  0.5f, -0.5f);

        color(0x5d00ffff);
        vertex(-0.5f,  0.5f,  0.5f);
        vertex(-0.5f,  0.5f, -0.5f);
        vertex(-0.5f, -0.5f, -0.5f);
        vertex(-0.5f, -0.5f,  0.5f);

        color(0x000c7dff);
        vertex( 0.5f,  0.5f, -0.5f);
        vertex( 0.5f,  0.5f,  0.5f);
        vertex( 0.5f, -0.5f,  0.5f);
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
    look_at(0.0f, 0.0f, -5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
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
