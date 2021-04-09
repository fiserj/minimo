#include <mnm/mnm.h>
#include <mnm/geometry.h>
#include <mnm/window.h>

static void setup(void)
{
    size(800, 600, WINDOW_DEFAULT);
    title("MiNiMo Test");
}

static void draw(void)
{
    projection();
    identity();
    perspective(60.0f, aspect(), 0.1f, 10.0f);
    // ortho(-aspect(), aspect(), -1.0f, 1.0f, -10.0f, 10.0f);

    view();
    identity();
    look_at(1.0f, 1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    model();
    identity();
    // translate(0.5f, 0.0f, 0.0f);

    begin();
    {
        color(0xff0000ff);
        vertex(-0.6f, -0.4f, 0.0f);

        color(0x00ff00ff);
        vertex(0.6f, -0.4f, 0.0f);

        color(0x0000ffff);
        vertex(0.0f, 0.6f, 0.0f);
    }
    end();
}

int main(int, char**)
{
    mnm_run(setup, draw, nullptr);

    return 0;
}
