#include <mnm/mnm.h>
#include <mnm/geometry.h>
#include <mnm/window.h>

static void setup(void)
{
    size(800, 600, WINDOW_FIXED_ASPECT);
    title("MiNiMo Test");
}

static void draw(void)
{
    // begin();
    // {
    //     color(0xff0000ff);
    //     vertex(-0.6f, -0.4f, 0.0f);

    //     color(0x00ff00ff);
    //     vertex(0.6f, -0.4f, 0.0f);

    //     color(0x0000ffff);
    //     vertex(0.0f, 0.6f, 0.0f);
    // }
    // end();
}

int main(int, char**)
{
    mnm_run(setup, draw, nullptr);

    return 0;
}
