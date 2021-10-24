#include <assert.h>                        // assert
#include <stdint.h>                        // uint*_t

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


// -----------------------------------------------------------------------------
// EXTERN HELPERS
// -----------------------------------------------------------------------------

struct GLFWwindow;

extern "C" GLFWwindow* mnm_get_window(void);

extern "C" ImFont* ImGui_Patch_ImFontAtlas_AddFontFromMemoryCompressedTTF
(
    ImFontAtlas*,
    const void*,
    unsigned int,
    float
);


// -----------------------------------------------------------------------------
// IMGUI BACKEND
// -----------------------------------------------------------------------------

struct Context
{
    bgfx::VertexLayout  vertex_layout;
    float               font_last_dpi   = 0.0f;
    float               font_cap_height = 10.0f;
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

        reset_font_atlas();

        view_id = bgfx::getCaps()->limits.maxViews - 2;
    }

    void cleanup()
    {
        if (bgfx::isValid(program_rgba   )) { bgfx::destroy(program_rgba   ); }
        if (bgfx::isValid(program_red    )) { bgfx::destroy(program_red    ); }
        if (bgfx::isValid(texture_sampler)) { bgfx::destroy(texture_sampler); }
        if (bgfx::isValid(font_texture   )) { bgfx::destroy(font_texture   ); }
    }

    inline void reset_font_atlas()
    {
        if (BX_LIKELY(dpi() == font_last_dpi))
        {
            return;
        }

        if (ImFontAtlas* atlas = ImGui::GetIO().Fonts)
        {
            font_last_dpi = dpi();

            // TODO : Perhaps we want to cache last N fonts (or at least all
            //        requested DPI variants of current font size)?
            atlas->ClearFonts();

            (void)ImGui_Patch_ImFontAtlas_AddFontFromMemoryCompressedTTF(
                atlas,
                font_compressed_data,
                font_compressed_size,
                font_cap_height * font_last_dpi
            );

            uint8_t* pixels = nullptr;
            int      width  = 0;
            int      height = 0;
            atlas->GetTexDataAsAlpha8(&pixels, &width, &height);

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

            ImGui::GetIO().FontGlobalScale = 1.0f / font_last_dpi;
        }
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

            // TODO : Investigate whether we could possibly succeed here, only
            //        to fail later on due to a multi-threaded app also using
            //        transient vetex buffer.
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

                // NOTE : Add back support for textures when actually needed.
                if (BX_LIKELY((cmd.ElemCount && !cmd.TextureId && !cmd.UserCallback)))
                {
                    const ImVec2 scale = io.DisplayFramebufferScale;

                    const uint16_t x = static_cast<uint16_t>(bx::max(cmd.ClipRect.x * scale.x, 0.0f));
                    const uint16_t y = static_cast<uint16_t>(bx::max(cmd.ClipRect.y * scale.y, 0.0f));
                    const uint16_t w = static_cast<uint16_t>(bx::min(cmd.ClipRect.z * scale.x, static_cast<float>(UINT16_MAX)) - x);
                    const uint16_t h = static_cast<uint16_t>(bx::min(cmd.ClipRect.w * scale.y, static_cast<float>(UINT16_MAX)) - y);

                    bgfx::setScissor(x, y, w, h);

                    bgfx::setTexture(0, texture_sampler, font_texture);

                    bgfx::setVertexBuffer(0, &tvb, 0, num_vertices);
                    bgfx::setIndexBuffer(&tib, offset, cmd.ElemCount);

                    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
                    bgfx::submit(view_id, program_red);
                }

                offset += cmd.ElemCount;
            }
        }
    }
};

static Context g_ctx;


// -----------------------------------------------------------------------------
// MINIMO CALLBACKS
// -----------------------------------------------------------------------------

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
    g_ctx.reset_font_atlas();

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


// -----------------------------------------------------------------------------
// MINIMO MAIN ENTRY
// -----------------------------------------------------------------------------

MNM_MAIN(nullptr, setup, update, cleanup);
