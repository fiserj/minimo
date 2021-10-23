#include <assert.h>                        // assert
#include <stdint.h>                        // uint*_t

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>                    // GLFWwindow

#include <bgfx/bgfx.h>                     // bgfx::*
#include <bgfx/embedded_shader.h>          // createEmbeddedShader

#include <bx/bx.h>                         // BX_*LIKELY, memCopy, min/max

#include <imgui.h>                         // ImGui::*
#include <imgui_impl_glfw.h>               // ImGui_ImplGlfw_*

#include <mnm/mnm.h>

#include <position_2d_color_texcoord_vs.h> // position_2d_color_texcoord_vs
#include <position_color_r_texcoord_fs.h>  // position_color_r_texcoord_fs
#include <position_color_texcoord_fs.h>    // position_color_texcoord_fs

#include "font.h"                          // font_compressed_*

extern "C" GLFWwindow* mnm_get_window(void);

// TODO : Non-destructively patch `imgui_draw.cpp` to get access to it.
extern unsigned int stb_decompress(unsigned char *, const unsigned char *, unsigned int);

enum struct BlendMode : uint8_t
{
    NONE,
    ADD,
    ALPHA,
    DARKEN,
    LIGHTEN,
    MULTIPLY,
    NORMAL,
    SCREEN,
    LINEAR_BURN,
};

enum struct TextureFormat : uint8_t
{
    RED,
    RGBA,
};

union Texture
{
    struct
    {
        bgfx ::TextureHandle handle;
        BlendMode            blend_mode;
        TextureFormat        texture_format;

    }           data;
    ImTextureID id;
};

struct Context
{
    bgfx::VertexLayout  vertex_layout;
    bgfx::ProgramHandle program_rgba    = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle program_red     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle texture_sampler = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle font_texture    = BGFX_INVALID_HANDLE;
    bgfx::ViewId        view_id         = UINT16_MAX;

    void init()
    {
        vertex_layout
            .begin()
            .add(bgfx::Attrib::Position , 2, bgfx::AttribType::Float)
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
        assert(bgfx::isValid(position_2d_color_texcoord_vs));

        bgfx::ShaderHandle position_color_r_texcoord_fs = bgfx::createEmbeddedShader(
            shaders,
            renderer,
            "position_color_r_texcoord_fs"
        );
        assert(bgfx::isValid(position_color_r_texcoord_fs));

        bgfx::ShaderHandle position_color_texcoord_fs = bgfx::createEmbeddedShader(
            shaders,
            renderer,
            "position_color_texcoord_fs"
        );
        assert(bgfx::isValid(position_color_texcoord_fs));

        program_rgba = bgfx::createProgram(position_2d_color_texcoord_vs, position_color_texcoord_fs);
        assert(bgfx::isValid(program_rgba));

        program_red = bgfx::createProgram(position_2d_color_texcoord_vs, position_color_r_texcoord_fs);
        assert(bgfx::isValid(program_red));

        texture_sampler = bgfx::createUniform("s_tex_color", bgfx::UniformType::Sampler);
        assert(bgfx::isValid(texture_sampler));

        ImGuiIO& io = ImGui::GetIO();

        // TODO : Font has to be rerasterized when DPI changes (or when user changes the size later on).

        (void)io.Fonts->AddFontFromMemoryCompressedTTF(
            font_compressed_data,
            font_compressed_size,
            10.0f * dpi() // TODO : Use the `cap_height()` calculation from `mnm.cpp`.
        );

        io.FontGlobalScale = 1.0f / dpi();

        uint8_t* pixels = nullptr;
        int      width  = 0;
        int      height = 0;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

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
        assert(bgfx::isValid(font_texture));

        view_id = bgfx::getCaps()->limits.maxViews - 2;
    }

    void cleanup()
    {
        if (bgfx::isValid(program_rgba   )) { bgfx::destroy(program_rgba   ); }
        if (bgfx::isValid(program_red    )) { bgfx::destroy(program_red    ); }
        if (bgfx::isValid(texture_sampler)) { bgfx::destroy(texture_sampler); }
        if (bgfx::isValid(font_texture   )) { bgfx::destroy(font_texture   ); }
    }

    void submit(const ImDrawData& draw_data)
    {
        const ImGuiIO& io     = ImGui::GetIO();
        const float    width  = io.DisplaySize.x * io.DisplayFramebufferScale.x;
        const float    height = io.DisplaySize.y * io.DisplayFramebufferScale.y;

        bgfx::setViewName(view_id, "ImGui");
        bgfx::setViewMode(view_id, bgfx::ViewMode::Sequential);

        {
            const float L = draw_data.DisplayPos.x;
            const float R = draw_data.DisplayPos.x + draw_data.DisplaySize.x;
            const float T = draw_data.DisplayPos.y;
            const float B = draw_data.DisplayPos.y + draw_data.DisplaySize.y;

            const float ortho[4][4] =
            {
                { 2.0f / (R - L)   , 0.0f             ,  0.0f, 0.0f },
                { 0.0f             , 2.0f / (T - B)   ,  0.0f, 0.0f },
                { 0.0f             , 0.0f             , -1.0f, 0.0f },
                { (R + L) / (L - R), (T + B) / (B - T),  0.0f, 1.0f },
            };

            bgfx::setViewTransform(view_id, nullptr, ortho);
            bgfx::setViewRect     (view_id,
                0, 0,
                static_cast<uint16_t>(width),
                static_cast<uint16_t>(height)
            );
        }

        for (int i = 0; i < draw_data.CmdListsCount; i++)
        {
            const ImDrawList * draw_list = draw_data.CmdLists[i];

            if (BX_UNLIKELY(!draw_list))
            {
                continue;
            }

            const uint32_t num_vertices = static_cast<uint32_t>(draw_list->VtxBuffer.size());
            const uint32_t num_indices  = static_cast<uint32_t>(draw_list->IdxBuffer.size());

            if (                     num_vertices < bgfx::getAvailTransientVertexBuffer(num_vertices, vertex_layout) ||
                (num_indices  > 0 && num_indices  < bgfx::getAvailTransientIndexBuffer (num_indices)))
            {
                // TODO : Add warning message.
                assert(false && "Full transient geometry.");
                break;
            }

            bgfx::TransientVertexBuffer tvb;
            bgfx::TransientIndexBuffer  tib;

            bgfx::allocTransientVertexBuffer(&tvb, num_vertices, vertex_layout);
            bgfx::allocTransientIndexBuffer (&tib, num_indices);

            bx::memCopy(tvb.data, draw_list->VtxBuffer.begin(), num_vertices * sizeof(ImDrawVert));
            bx::memCopy(tib.data, draw_list->IdxBuffer.begin(), num_indices  * sizeof(ImDrawIdx ));

            uint32_t offset = 0;

            for (int j = 0; j < draw_list->CmdBuffer.size(); j++)
            {
                const ImDrawCmd & cmd = draw_list->CmdBuffer[j];

                if (cmd.UserCallback)
                {
                    cmd.UserCallback(draw_list, &cmd);
                }
                else if (cmd.ElemCount)
                {
                    uint64_t            state   = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
                    bgfx::TextureHandle texture = font_texture;
                    bgfx::ProgramHandle program = program_red;

                    if (cmd.TextureId)
                    {
                        static const uint64_t blend_states[] =
                        {
                            0,
                            BGFX_STATE_BLEND_ADD,
                            BGFX_STATE_BLEND_ALPHA,
                            BGFX_STATE_BLEND_DARKEN,
                            BGFX_STATE_BLEND_LIGHTEN,
                            BGFX_STATE_BLEND_MULTIPLY,
                            BGFX_STATE_BLEND_NORMAL,
                            BGFX_STATE_BLEND_SCREEN,
                            BGFX_STATE_BLEND_LINEAR_BURN,
                        };

                        Texture convert;
                        convert.id = cmd.TextureId;

                        texture = convert.data.handle;
                        state  |= blend_states[static_cast<uint8_t>(convert.data.blend_mode)];

                        switch (convert.data.texture_format)
                        {
                        case TextureFormat::RGBA:
                            program = program_rgba;
                            break;
                        default:;
                        }
                    }
                    else
                    {
                        state |= BGFX_STATE_BLEND_ALPHA;
                    }

                    const ImVec2 scale = io.DisplayFramebufferScale;

                    const uint16_t x = static_cast<uint16_t>(bx::max(cmd.ClipRect.x * scale.x, 0.0f));
                    const uint16_t y = static_cast<uint16_t>(bx::max(cmd.ClipRect.y * scale.y, 0.0f));
                    const uint16_t w = static_cast<uint16_t>(bx::min(cmd.ClipRect.z * scale.x, static_cast<float>(UINT16_MAX)) - x);
                    const uint16_t h = static_cast<uint16_t>(bx::min(cmd.ClipRect.w * scale.y, static_cast<float>(UINT16_MAX)) - y);

                    bgfx::setState(state);
                    bgfx::setScissor(x, y, w, h);
                    bgfx::setTexture(0, texture_sampler, texture);
                    bgfx::setVertexBuffer(0, &tvb, 0, num_vertices);
                    bgfx::setIndexBuffer(&tib, offset, cmd.ElemCount);
                    bgfx::submit(view_id, program);
                }

                offset += cmd.ElemCount;
            }
        }
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

static void update()
{
    do_gui();

    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    // ...
}

MNM_MAIN(nullptr, setup, update, cleanup);
