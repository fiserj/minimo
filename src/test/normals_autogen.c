#include <mnm/mnm.h>

#include <math.h>  // cosf, sinf

#define CUBE_MESH 1

static void torus(int, int);

static void scene(void);

static void setup(void)
{
    title("Normals Autogeneration Example");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    begin_mesh(CUBE_MESH, PRIMITIVE_QUADS | VERTEX_NORMAL | GENEREATE_FLAT_NORMALS);
    torus(10, 25);
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
    look_at(0.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    view();

    identity();
    scene();
}

void scene(void)
{
    rotate_x (((float)elapsed() + 0.21f) * 57.2958f);
    rotate_y (((float)elapsed() + 0.37f) * 57.2958f);

    mesh(CUBE_MESH);
}

static void torus_vertex(int radial_resolution, int tubular_resolution, int index)
{
    const float radius    = 0.50f;
    const float thickness = 0.15f;

    const int i = index / tubular_resolution;
    const int j = index % tubular_resolution;

    const float u =  6.28318530718f * j / tubular_resolution;
    const float v =  6.28318530718f * i / radial_resolution;

    const float x = (radius + thickness * cosf(v)) * cosf(u);
    const float y = (radius + thickness * cosf(v)) * sinf(u);
    const float z = thickness * sinf(v);

    vertex(x, y, z);
}

// https://www.danielsieger.com/blog/2021/05/03/generating-primitive-shapes.html
static void torus(int radial_resolution, int tubular_resolution)
{
    for (int i = 0; i < radial_resolution; i++)
    {
        const int i_next = (i + 1) % radial_resolution;

        for (int j = 0; j < tubular_resolution; j++)
        {
            const int j_next = (j + 1) % tubular_resolution;

            const int i0 = i * tubular_resolution + j;
            const int i1 = i * tubular_resolution + j_next;
            const int i2 = i_next * tubular_resolution + j_next;
            const int i3 = i_next * tubular_resolution + j;

            torus_vertex(radial_resolution, tubular_resolution, i0);
            torus_vertex(radial_resolution, tubular_resolution, i1);
            torus_vertex(radial_resolution, tubular_resolution, i2);
            torus_vertex(radial_resolution, tubular_resolution, i3);
        }
    }
}

MNM_MAIN(0, setup, draw, 0);
