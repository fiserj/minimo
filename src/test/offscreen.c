#include <mnm/mnm.h>

#define MESH_CUBE             1
#define MESH_QUAD             2

#define TEXTURE_COLOR         1
#define TEXTURE_DEPTH         2

#define FRAMEBUFFER_OFFSCREEN 1

#define PASS_OFFSCREEN        1

static void cube(void);

static void offscreen_pass(void);

static void default_pass(void);

static void setup(void)
{
    title("Framebuffer Example");

    create_texture(TEXTURE_COLOR, TEXTURE_CLAMP | TEXTURE_TARGET               , SIZE_HALF, SIZE_HALF);
    create_texture(TEXTURE_DEPTH, TEXTURE_CLAMP | TEXTURE_TARGET | TEXTURE_D32F, SIZE_HALF, SIZE_HALF);

    begin_framebuffer(FRAMEBUFFER_OFFSCREEN);
    texture(TEXTURE_COLOR);
    texture(TEXTURE_DEPTH);
    end_framebuffer();

    begin_static(MESH_CUBE, PRIMITIVE_QUADS | VERTEX_COLOR);
    cube();
    end();
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    begin_pass(PASS_OFFSCREEN);
    {
        offscreen_pass();
    }
    end_pass();

    default_pass();
}

void offscreen_pass(void)
{
    clear_color(0xccffccff);

    framebuffer(FRAMEBUFFER_OFFSCREEN);

    projection();
    identity();
    perspective(60.0f, aspect(), 0.1f, 100.0f);

    view();
    identity();
    look_at(0.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    model();
    identity();

    rotate_x(((float)elapsed() + 1.0f) * 57.2958f);
    rotate_y(((float)elapsed() + 2.0f) * 57.2958f);
    cube();

    mesh(MESH_CUBE);
}

void default_pass(void)
{
    clear_color(0x333333ff);

    projection();
    ortho(-aspect(), aspect(), -1.0f, 1.0f, 1.0f, -1.0f);

    view();
    identity();

    model();
    identity();

    begin_transient(MESH_QUAD, PRIMITIVE_QUADS | VERTEX_TEXCOORD);
    {
        const float a = aspect() * 0.5f;

        texcoord(0.0f, 0.0f);
        vertex(-a, 0.5f, 0.0f);

        texcoord(0.0f, 1.0f);
        vertex(-a, -0.5f, 0.0f);

        texcoord(1.0f, 1.0f);
        vertex(a, -0.5f, 0.0f);

        texcoord(1.0f, 0.0f);
        vertex(a, 0.5f, 0.0f);
    }
    end();

    mesh(MESH_QUAD);
}

void cube(void)
{
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

MNM_MAIN(0, setup, draw, 0);
