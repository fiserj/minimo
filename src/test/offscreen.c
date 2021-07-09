#include <mnm/mnm.h>

#define MESH_CUBE             1
#define MESH_QUAD             2

#define TEXTURE_COLOR         1
#define TEXTURE_DEPTH         2

#define FRAMEBUFFER_OFFSCREEN 1

#define PASS_OFFSCREEN        1
#define PASS_DEFAULT          2

#define SIZE_OFFSCREEN        512

static void cube(void);

static void setup(void)
{
    title("Framebuffer Example");

    create_texture(TEXTURE_COLOR, TEXTURE_CLAMP | TEXTURE_TARGET               , SIZE_OFFSCREEN, SIZE_OFFSCREEN);
    create_texture(TEXTURE_DEPTH, TEXTURE_CLAMP | TEXTURE_TARGET | TEXTURE_D32F, SIZE_OFFSCREEN, SIZE_OFFSCREEN);

    begin_framebuffer(FRAMEBUFFER_OFFSCREEN);
    texture(TEXTURE_COLOR);
    texture(TEXTURE_DEPTH);
    end_framebuffer();

    begin_mesh(MESH_CUBE, PRIMITIVE_QUADS | VERTEX_COLOR);
    cube();
    end_mesh();

    pass(PASS_OFFSCREEN);
    clear_color(0x33aa33ff);
    clear_depth(1.0f);
    framebuffer(FRAMEBUFFER_OFFSCREEN);
    viewport(0, 0, SIZE_OFFSCREEN, SIZE_OFFSCREEN);

    pass(PASS_DEFAULT);
    clear_color(0x000000ff);
    clear_depth(1.0f);
    full_viewport();
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    pass(PASS_OFFSCREEN);
    {
        identity();
        perspective(60.0f, 1.0f, 0.1f, 100.0f);
        projection();

        identity();
        look_at(0.0f, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        view();

        identity();
        rotate_x(((float)elapsed() + 1.0f) * 57.2958f);
        rotate_y(((float)elapsed() + 2.0f) * 57.2958f);

        mesh(MESH_CUBE);
    }

    pass(PASS_DEFAULT);
    {
        // identity();
        // ortho(-aspect(), aspect(), -1.0f, 1.0f, 1.0f, -1.0f);
        // projection();

        identity();
        perspective(60.0f, aspect(), 0.1f, 100.0f);
        projection();

        identity();
        look_at(0.0f, 0.0f, 2.5f, 0.0f, 0.2f, 0.0f, 0.0f, 1.0f, 0.0f);
        view();

        identity();

        begin_mesh(MESH_QUAD, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR | VERTEX_TEXCOORD);
        {
            color(0xffffffff);

            texcoord(0.0f, 0.0f);
            vertex(-0.5f, 1.0f, 0.0f);

            texcoord(0.0f, 1.0f);
            vertex(-0.5f, 0.0f, 0.0f);

            texcoord(1.0f, 1.0f);
            vertex(0.5f, 0.0f, 0.0f);

            texcoord(1.0f, 0.0f);
            vertex(0.5f, 1.0f, 0.0f);


            color(0x444444ff);
            texcoord(0.0f, 1.0f);
            vertex(-0.5f, 0.0f, 0.0f);

            color(0x000000ff);
            texcoord(0.0f, 0.0f);
            vertex(-0.5f, -0.8f, 0.0f);

            color(0x000000ff);
            texcoord(1.0f, 0.0f);
            vertex(0.5f, -0.8f, 0.0f);

            color(0x444444ff);
            texcoord(1.0f, 1.0f);
            vertex(0.5f, 0.0f, 0.0f);
        }
        end_mesh();

        identity();
        texture(TEXTURE_COLOR);
        mesh(MESH_QUAD);
    }
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
