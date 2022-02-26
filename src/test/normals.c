#include <mnm/mnm.h>

#define CUBE_MESH 1

static void cube(void);

static void setup(void)
{
    title("Normals Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    begin_mesh(CUBE_MESH, PRIMITIVE_QUADS | VERTEX_NORMAL);
    cube();
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
    look_at(0.0f, 0.0f, -3.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    view();

    identity();
    rotate_x(((float)elapsed() + 0.21f) * 57.2958f);
    rotate_y(((float)elapsed() + 0.37f) * 57.2958f);
    mesh(CUBE_MESH);
}

static void cube(void)
{
    normal( 0.0f,  1.0f,  0.0f);
    vertex( 0.5f,  0.5f, -0.5f);
    vertex(-0.5f,  0.5f, -0.5f);
    vertex(-0.5f,  0.5f,  0.5f);
    vertex( 0.5f,  0.5f,  0.5f);

    normal( 0.0f, -1.0f,  0.0f);
    vertex( 0.5f, -0.5f,  0.5f);
    vertex(-0.5f, -0.5f,  0.5f);
    vertex(-0.5f, -0.5f, -0.5f);
    vertex( 0.5f, -0.5f, -0.5f);

    normal( 0.0f,  0.0f,  1.0f);
    vertex( 0.5f,  0.5f,  0.5f);
    vertex(-0.5f,  0.5f,  0.5f);
    vertex(-0.5f, -0.5f,  0.5f);
    vertex( 0.5f, -0.5f,  0.5f);

    normal( 0.0f,  0.0f, -1.0f);
    vertex( 0.5f, -0.5f, -0.5f);
    vertex(-0.5f, -0.5f, -0.5f);
    vertex(-0.5f,  0.5f, -0.5f);
    vertex( 0.5f,  0.5f, -0.5f);

    normal(-1.0f,  0.0f,  0.0f);
    vertex(-0.5f,  0.5f,  0.5f);
    vertex(-0.5f,  0.5f, -0.5f);
    vertex(-0.5f, -0.5f, -0.5f);
    vertex(-0.5f, -0.5f,  0.5f);

    normal( 1.0f,  0.0f,  0.0f);
    vertex( 0.5f,  0.5f, -0.5f);
    vertex( 0.5f,  0.5f,  0.5f);
    vertex( 0.5f, -0.5f,  0.5f);
    vertex( 0.5f, -0.5f, -0.5f);
}

MNM_MAIN(0, setup, draw, 0);
