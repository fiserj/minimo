#include <stdio.h>

#include <mnm/mnm.h>

typedef struct
{
    float        x, y, z;
    unsigned int color;

} Vertex;

static void task_func(void* data)
{
    const int i = *((int*)data);

    printf("[%i] Start\n", i);

    sleep_for(i);

    printf("[%i] End\n", i);
}

static void quad(const Vertex* vertices, int i0, int i1, int i2, int i3)
{
    const Vertex* v0 = vertices + i0;
    const Vertex* v1 = vertices + i1;
    const Vertex* v2 = vertices + i2;
    const Vertex* v3 = vertices + i3;

    color (v0->color);
    vertex(v0->x, v0->y, v0->z);

    color (v1->color);
    vertex(v1->x, v1->y, v1->z);

    color (v2->color);
    vertex(v2->x, v2->y, v2->z);

    color (v0->color);
    vertex(v0->x, v0->y, v0->z);

    color (v2->color);
    vertex(v2->x, v2->y, v2->z);

    color (v3->color);
    vertex(v3->x, v3->y, v3->z);
}

static void cube(void)
{
    static const Vertex vertices[] =
    {
        {  0.5f,  0.5f, -0.5f, 0xff00ffff }, // 0
        { -0.5f,  0.5f, -0.5f, 0x0000ffff }, // 1
        { -0.5f,  0.5f,  0.5f, 0x000000ff }, // 2
        {  0.5f,  0.5f,  0.5f, 0xff0000ff }, // 3
        {  0.5f, -0.5f,  0.5f, 0xffff00ff }, // 4
        { -0.5f, -0.5f,  0.5f, 0x00ff00ff }, // 5
        { -0.5f, -0.5f, -0.5f, 0x00ffffff }, // 6
        {  0.5f, -0.5f, -0.5f, 0xffffffff }, // 7
    };

    quad(vertices, 0, 1, 2, 3);
    quad(vertices, 4, 5, 6, 7);
    quad(vertices, 3, 2, 5, 4);
    quad(vertices, 7, 6, 1, 0);
    quad(vertices, 2, 1, 6, 5);
    quad(vertices, 0, 3, 4, 7);
}

static void scene(int cube_cache_id = 0)
{
    for (float y = 0.0f; y < 11.0f; y += 1.0f)
    for (float x = 0.0f; x < 11.0f; x += 1.0f)
    {
        push();

        rotate_x (((float)elapsed() + x * 0.21f) * 57.2958f);
        rotate_y (((float)elapsed() + y * 0.37f) * 57.2958f);
        translate(-7.5f + x * 1.5f, -7.5f + y * 1.5f, 0.0f);

        cube_cache_id ? cache(cube_cache_id) : cube();

        pop();
    }
}

static const int CUBE_ID = 1;

static void setup(void)
{
    size(800, 600, WINDOW_DEFAULT);
    title("MiNiMo Test");

    begin_cached(CUBE_ID);
    {
        cube();
    }
    end();
}

static void draw(void)
{
    static bool vsync_on   = false;
    static bool caching_on = true;

    if (key_down(KEY_ESCAPE))
    {
        quit();
        return;
    }

    if (key_down('V'))
    {
        vsync_on = !vsync_on;
        vsync(vsync_on);
    }

    if (key_down('C'))
    {
        caching_on = !caching_on;
    }

    if (mouse_down(MOUSE_LEFT))
    {
        printf("(%4i, %4i)\n", mouse_x(), mouse_y());
    }

    projection();
    identity();
    perspective(60.0f, aspect(), 0.1f, 100.0f);

    if (key_down(KEY_SPACE))
    {
        static int tids[] = { 1, 2, 3 };
        task(task_func, tids + 0);
        task(task_func, tids + 1);
        task(task_func, tids + 2);
    }

    view();
    identity();
    look_at(
        0.0f, 0.0f, -17.5f,
        0.0f, 0.0f,   0.0f,
        0.0f, 1.0f,   0.0f
    );

    model();
    identity();

    if (caching_on)
    {
        scene(CUBE_ID);
    }
    else
    {
        begin();
        {
            scene();
        }
        end();
    }
}

int main(int, char**)
{
    mnm_run(setup, draw, 0);

    return 0;
}
