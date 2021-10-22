#include <stdint.h>                        // uint*_t

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>                    // GLFWwindow

#include <bgfx/bgfx.h>                     // bgfx::*
#include <bgfx/embedded_shader.h>          // createEmbeddedShader

#include <imgui.h>                         // ImGui::*
#include <imgui_impl_glfw.h>               // ImGui_ImplGlfw_*

#include <mnm/mnm.h>

#include <position_2d_color_texcoord_vs.h> // position_2d_color_texcoord_vs
#include <position_color_r_texcoord_fs.h>  // position_color_r_texcoord_fs
#include <position_color_texcoord_fs.h>    // position_color_texcoord_fs

extern "C" GLFWwindow* mnm_get_window(void);

struct Context
{
    bgfx::VertexLayout  vertex_layout;
    bgfx::ProgramHandle program_rgba    = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle program_r       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle texture_sampler = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle font_texture    = BGFX_INVALID_HANDLE;
    bgfx::ViewId        view_id         = UINT16_MAX;

    void init()
    {
        vertex_layout
            .begin()
            .add(bgfx::Attrib::TexCoord3, 2, bgfx::AttribType::Float) // 2D position.
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0   , 4, bgfx::AttribType::Uint8, true)
            .end();

        
        const bgfx::RendererType::Enum renderer  = bgfx::getRendererType();
        const bgfx::EmbeddedShader     shaders[] =
        {
            BGFX_EMBEDDED_SHADER(position_2d_color_texcoord_vs),
            BGFX_EMBEDDED_SHADER(position_color_r_texcoord_fs ),
            BGFX_EMBEDDED_SHADER(position_color_texcoord_fs   ),

            BGFX_EMBEDDED_SHADER_END()
        };

        bgfx::ShaderHandle position_2d_color_texcoord_vs = bgfx::createEmbeddedShader(
            shaders,
            renderer,
            "position_2d_color_texcoord_vs"
        );

        bgfx::ShaderHandle position_color_r_texcoord_fs = bgfx::createEmbeddedShader(
            shaders,
            renderer,
            "position_color_r_texcoord_fs"
        );

        bgfx::ShaderHandle position_color_texcoord_fs = bgfx::createEmbeddedShader(
            shaders,
            renderer,
            "position_color_texcoord_fs"
        );

        program_rgba = bgfx::createProgram(position_2d_color_texcoord_vs, position_color_texcoord_fs);

        program_r = bgfx::createProgram(position_2d_color_texcoord_vs, position_color_r_texcoord_fs);

        texture_sampler = bgfx::createUniform("s_tex_color", bgfx::UniformType::Sampler);

        uint8_t* pixels = nullptr;
        int      width  = 0;
        int      height = 0;
        ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

        font_texture = bgfx::createTexture2D
        (
            static_cast<uint16_t>(width ),
            static_cast<uint16_t>(height),
            false,
            1,
            bgfx::TextureFormat::R8,
            0,
            bgfx::copy(pixels, static_cast<uint32_t>(width) * static_cast<uint32_t>(height))
        );

        view_id = bgfx::getCaps()->limits.maxViews - 2;
    }

    void cleanup()
    {
        if (bgfx::isValid(program_rgba   )) { bgfx::destroy(program_rgba   ); }
        if (bgfx::isValid(program_r      )) { bgfx::destroy(program_r      ); }
        if (bgfx::isValid(texture_sampler)) { bgfx::destroy(texture_sampler); }
        if (bgfx::isValid(font_texture   )) { bgfx::destroy(font_texture   ); }
    }

    void submit(const ImDrawData& draw_data)
    {
        // ...
    }
};

static Context g_ctx;

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

    (void)ImGui_ImplGlfw_InitForOther(mnm_get_window(), true);
    g_ctx.init();
}

static void cleanup()
{
    g_ctx.cleanup();
    ImGui_ImplGlfw_Shutdown();
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

    const ImDrawData* draw_data = ImGui::GetDrawData();

    if (BX_LIKELY(draw_data))
    {
        g_ctx.submit(*draw_data);
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
