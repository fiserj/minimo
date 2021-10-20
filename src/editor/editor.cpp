#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>      // GLFWwindow

#include <imgui.h>           // ImGui::*
#include <imgui_impl_glfw.h> // ImGui_ImplGlfw_*

#include <mnm/mnm.h>

#define IMGUI_FONT_ID 1023

#define IMGUI_PASS_ID 63

extern "C" GLFWwindow* mnm_get_window(void);

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x303030ff);
    clear_depth(1.0f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename  = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOther(mnm_get_window(), true);

    unsigned char* pixels = nullptr;
    int            width  = 0;
    int            height = 0;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    load_texture(IMGUI_FONT_ID, TEXTURE_R8, width, height, width, pixels);

    // TODO : Setup ImGui pass. This will involve either exposing
    //        `bgfx::setViewMode` in some form, or directly using BGFX.
}

static void cleanup()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

static void submit_gui_draw_data(const ImDrawData& draw_data)
{
    
}

static void do_gui()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Hello, world!"))
    {
        ImGui::Button("Button");
    }
    ImGui::End();

    ImGui::Render();

    if (const ImDrawData* draw_data = ImGui::GetDrawData())
    {
        submit_gui_draw_data(*draw_data);
    }
}

static void draw()
{
    do_gui();

    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    // ...
}

MNM_MAIN(nullptr, setup, draw, cleanup);
