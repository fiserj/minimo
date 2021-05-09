#include <mnm/mnm.h>

#define SCENE_ID 1

static void cube(void);

static void scene(void);

static void setup(void)
{
    title("Transient Geometry Example");
}

static void draw(void)
{
    projection();
    identity();
    perspective(60.0f, aspect(), 0.1f, 100.0f);

    view();
    identity();
    look_at(0.0f, 0.0f, -17.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    model();
    identity();

    begin_transient(SCENE_ID, COLOR);
    scene();
    end();

    mesh(SCENE_ID);
}

void scene(void)
{
    for (float y = 0.0f; y < 11.0f; y += 1.0f)
    for (float x = 0.0f; x < 11.0f; x += 1.0f)
    {
        push();

        rotate_x (((float)elapsed() + x * 0.21f) * 57.2958f);
        rotate_y (((float)elapsed() + y * 0.37f) * 57.2958f);
        translate(-7.5f + x * 1.5f, -7.5f + y * 1.5f, 0.0f);

        cube();

        pop();
    }
}

void cube(void)
{
    // ...
}

MNM_MAIN(setup, draw, 0);
