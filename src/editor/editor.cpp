#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>      // GLFWwindow

#include <imgui.h>           // ImGui::*
#include <imgui_impl_glfw.h> // ImGui_ImplGlfw_*

#include <mnm/mnm.h>

extern "C" GLFWwindow* mnm_get_window(void);

static GLFWwindow* g_window = nullptr;

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x303030ff);
    clear_depth(1.0f);

    g_window = mnm_get_window();

    // ...
}

static void cleanup()
{
    // ...
}

static void draw()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    // ...
}

MNM_MAIN(nullptr, setup, draw, cleanup);
