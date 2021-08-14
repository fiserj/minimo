#include <mnm/mnm.h>

#define CUBE_MESH      1

#define CUBE_INSTANCES 1

static void cube(void);

static void scene(void);

static void setup(void)
{
    title("Instancing Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    begin_mesh(CUBE_MESH, PRIMITIVE_QUADS | VERTEX_COLOR);
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
    look_at(0.0f, 0.0f, -17.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    view();

    identity();
    scene();

    instances(CUBE_INSTANCES);
    mesh(CUBE_MESH);
}

void scene(void)
{

    begin_instancing(CUBE_INSTANCES, INSTANCE_TRANSFORM);

    for (float y = 0.0f; y < 11.0f; y += 1.0f)
    for (float x = 0.0f; x < 11.0f; x += 1.0f)
    {
        push();

        rotate_x (((float)elapsed() + x * 0.21f) * 57.2958f);
        rotate_y (((float)elapsed() + y * 0.37f) * 57.2958f);
        translate(-7.5f + x * 1.5f, -7.5f + y * 1.5f, 0.0f);

        instance(0);

        pop();
    }

    end_instancing();
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
