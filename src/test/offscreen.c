#include <mnm/mnm.h>

#define CUBE_ID       1
#define QUAD_ID       2

#define COLOR_TEX_ID  1
#define DEPTH_TEX_ID  2

#define OFF_FBUF_ID   1

#define OFF_PASS_ID   1

static void cube(void);

static void offscreen_pass(void);

static void main_pass(void);

static void setup(void)
{
    title("Framebuffer Example");

    clear_color(0x333333ff);

    create_texture(COLOR_TEX_ID, TEXTURE_CLAMP | TEXTURE_FB               , SIZE_HALF, SIZE_HALF);
    create_texture(DEPTH_TEX_ID, TEXTURE_CLAMP | TEXTURE_FB | TEXTURE_D32F, SIZE_HALF, SIZE_HALF);

    begin_framebuffer(OFF_FBUF_ID);
    {
        texture(COLOR_TEX_ID);
        texture(DEPTH_TEX_ID);
    }
    end_framebuffer();

    begin_pass(OFF_PASS_ID);
    {
        clear_color(0xccffccff);

        framebuffer(OFF_FBUF_ID);
    }
    end_pass();
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    begin_pass(OFF_PASS_ID);
    offscreen_pass();
    end_pass();

    main_pass();
}

static void offscreen_pass(void)
{
    projection();
    identity();
    perspective(60.0f, aspect(), 0.1f, 100.0f);

    view();
    identity();
    look_at(0.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    model();
    identity();

    begin_transient(CUBE_ID, PRIMITIVE_QUADS | VERTEX_COLOR);
    {
        rotate_x(((float)elapsed() + 1.0f) * 57.2958f);
        rotate_y(((float)elapsed() + 2.0f) * 57.2958f);
        cube();
    }
    end();

    mesh(CUBE_ID);
}

static void main_pass(void)
{
    projection();
    ortho(-aspect(), aspect(), -1.0f, 1.0f, 1.0f, -1.0f);

    view();
    identity();

    model();
    identity();

    begin_transient(QUAD_ID, PRIMITIVE_QUADS | VERTEX_TEXCOORD);
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

    mesh(QUAD_ID);
}

static void cube(void)
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
