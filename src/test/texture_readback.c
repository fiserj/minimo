#include <mnm/mnm.h>

#define PASS_OFFSCREEN 1
#define PASS_DEFAULT   2

#define FRAMEBUFFER_ID 1

#define TEXTURE_COLOR  1
#define TEXTURE_DEPTH  2

#define TRIANGLE_ID    1

static void setup(void)
{
    title("Texture Readback Example");

    create_texture(TEXTURE_COLOR, TEXTURE_CLAMP | TEXTURE_TARGET               , SIZE_EQUAL, SIZE_EQUAL);
    create_texture(TEXTURE_DEPTH, TEXTURE_CLAMP | TEXTURE_TARGET | TEXTURE_D32F, SIZE_EQUAL, SIZE_EQUAL);

    begin_framebuffer(FRAMEBUFFER_ID);
    texture(TEXTURE_COLOR);
    texture(TEXTURE_DEPTH);
    end_framebuffer();

    pass(PASS_OFFSCREEN);
    clear_color(0x333333ff);
    clear_depth(1.0f);
    framebuffer(FRAMEBUFFER_ID);
    full_viewport();

    pass(PASS_DEFAULT);
    clear_color(0x00ff00ff);
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
        ortho(-aspect(), aspect(), -1.0f, 1.0f, 1.0f, -1.0f);
        projection();

        identity();
        rotate_z((float)elapsed() * -50.0f);

        begin_mesh(TRIANGLE_ID, MESH_TRANSIENT | VERTEX_COLOR);
        {
            color(0xff0000ff);
            vertex(-0.6f, -0.4f, 0.0f);

            color(0x00ff00ff);
            vertex(0.6f, -0.4f, 0.0f);

            color(0x0000ffff);
            vertex(0.0f, 0.6f, 0.0f);
        }
        end_mesh();

        identity();
        mesh(TRIANGLE_ID);
    }

    pass (PASS_DEFAULT);
    {
        // TODO : This should use dynamically allocated memory eventually.
        static unsigned char data[1920 * 1080 * 4 * 4] = { 0 };
        static int           saved                     =   0  ;

        if (frame() == 0)
        {
            read_texture(TEXTURE_COLOR, data);
        }

        if (!saved && readable(TEXTURE_COLOR))
        {
            // TODO : Save `data` into an image file.
            saved = 1;
        }
    }
}

MNM_MAIN(0, setup, draw, 0);
