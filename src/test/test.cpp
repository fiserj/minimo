#include <mnm/mnm.h>

typedef struct
{
    float        x, y, z;
    unsigned int color;

} Vertex;

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

static void cube()
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

static void setup(void)
{
    size(800, 600, WINDOW_DEFAULT);
    title("MiNiMo Test");
}

static void draw(void)
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
        return;
    }

    projection();
    identity();
    perspective(60.0f, aspect(), 0.1f, 10.0f);

    static float y = 2.0f;
    if (key_down(KEY_UP  )) { y += 0.5f; }
    if (key_down(KEY_DOWN)) { y -= 0.5f; }
    if (key_up  ('X'     )) { y  = 0.0f; }

    view();
    identity();
    look_at(2.0f, y, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    model();
    identity();

    static float angle = 0.0f;
    angle += 0.5f;
    rotate_y(angle);

    begin();
    {
        cube();
    }
    end();
}

int main(int, char**)
{
    mnm_run(setup, draw, nullptr);

    return 0;
}
