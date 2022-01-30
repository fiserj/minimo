#include <assert.h>               // assert
#include <stddef.h>               // offsetof
#include <stdint.h>               // uint*_t

#include <vector>                 // vector

#include <bgfx/embedded_shader.h> // BGFX_EMBEDDED_SHADER* (not really needed here, but necessary due to the included shader headers)

#include <bx/bx.h>                // BX_COUNTOF, BX_LIKELY, memCopy, min/max
#include <bx/hash.h>              // hash, HashAdler32
#include <bx/math.h>              // ceil, floor, mod
#include <bx/string.h>            // snprintf, strLen
#include <bx/timer.h>             // getHPCounter, getHPFrequency

#include <utf8.h>                 // utf8codepoint, utf8nlen, utf8size_lazy

#include <mnm/mnm.h>

#include <shaders/text_fs.h>      // text_fs
#include <shaders/text_vs.h>      // text_vs


// -----------------------------------------------------------------------------
// UTILITY MACROS
// -----------------------------------------------------------------------------

#define ID (__COUNTER__)

// TODO : Better assert.
#define ASSERT(cond) assert(cond)


// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T>
using Vector = std::vector<T>;


// -----------------------------------------------------------------------------
// INTERNAL INCLUDES
// -----------------------------------------------------------------------------

#include "mnm_text_editor_state.cpp"

#include "editor_font.h" // g_font_*
#include "editor_gui.h"  // Context, Editor
#include "editor_ted.h"
#include "editor_tcc.h"


// ------------/_\--------------------------------------------------------------
// GLOBALS \__(ovo)__/
// ------------| |--------------------------------------------------------------

static gui::Context g_gui;

static TextEditor g_editor;


// -----------------------------------------------------------------------------
// HELPERS
// -----------------------------------------------------------------------------

// TODO : This should be handled by the editor itself.
static gui::Rect script_viewport(const TextEditor& editor)
{
    constexpr float divider_thickness =  4.0f;

    gui::Rect viewport = {};

    switch (editor.display_mode)
    {
    case TextEditor::DisplayMode::RIGHT:
        viewport = { 0.0f, 0.0f, editor.split_x, height() };
        break;
    case TextEditor::DisplayMode::LEFT:
        viewport = { editor.split_x + divider_thickness, 0.0f, width(), height() };
        break;
    case TextEditor::DisplayMode::OVERLAY:
        viewport = { 0.0f, 0.0f, width(), height() };
        break;
    default:
        ASSERT(false);
    }

    const float dpi = ::dpi();

    viewport.x0 *= dpi;
    viewport.y0 *= dpi;
    viewport.x1 *= dpi;
    viewport.y1 *= dpi;

    ASSERT(viewport.x0 == static_cast<int>(viewport.x0));
    ASSERT(viewport.y0 == static_cast<int>(viewport.y0));
    ASSERT(viewport.x1 == static_cast<int>(viewport.x1));
    ASSERT(viewport.y1 == static_cast<int>(viewport.y1));

    return viewport;
}

struct ChangeWatch
{
    using HashT = bx::HashAdler32; // Or `bx::HashCrc32`, need to profile.

    double   freq = 1.0 / bx::getHPFrequency();
    double   wait = 0.5;
    int64_t  time = 0;
    uint32_t hash = 0;
    uint32_t last = 0;

    void init(const char* data, size_t size)
    {
        hash = bx::hash<HashT>(data, static_cast<uint32_t>(size));
        time = bx::getHPCounter();
    }

    // `true` is returned when content changed, but stayed the same for two
    // consecutive update calls. Done to allow for evaluation after a chunk of
    // text is being finished.
    //
    // But ideally, `update` should just be not called every frame, but only
    // with certain delay from last codepoint event.
    bool update(const char* data, size_t size)
    {
        const int64_t now     = bx::getHPCounter();
        const double  elapsed = (now - time) * freq;
        bool          updated = false;

        if (elapsed >= wait)
        {
            const uint32_t check = bx::hash<HashT>(data, static_cast<uint32_t>(size));

            if (check != hash)
            {
                if (check == last)
                {
                    hash    = check;
                    last    = 0;
                    updated = true;
                }
                else
                {
                    last = check;
                }
            }

            time = now;
        }

        return updated;
    }
};

static ChangeWatch g_watch;


// -----------------------------------------------------------------------------
// MINIMO CALLBACKS
// -----------------------------------------------------------------------------

static void setup()
{
    // TODO : Change when limits can be queried.
    gui::Resources& res = g_gui.resources;
    res.font_atlas              = 127;
    res.framebuffer_glyph_cache = 127;
    res.mesh_tmp_text           = 4093;
    res.mesh_gui_rects          = 4094;
    res.mesh_gui_text           = 4095;
    res.pass_glyph_cache        = 62;
    res.pass_gui                = 63;
    res.program_gui_text        = 127;
    res.texture_glyph_cache     = 1022;
    res.texture_tmp_atlas       = 1023;
    res.uniform_text_info       = 255;

    vsync(1);

    title("MiNiMo Editor");

    pass(res.pass_gui);

//     clear_color(0x303030ff);
//     clear_depth(1.0f);

    create_font(res.font_atlas, g_font_data);

    // TODO : Add `MiNiMo` support for backend-specific shader selection.
#if BX_PLATFORM_OSX
    create_shader(res.program_gui_text, text_vs_mtl , sizeof(text_vs_mtl ), text_fs_mtl , sizeof(text_fs_mtl ));
#elif BX_PLATFORM_WINDOWS
    create_shader(res.program_gui_text, text_vs_dx11, sizeof(text_vs_dx11), text_fs_dx11, sizeof(text_fs_dx11));
#endif

    create_uniform(res.uniform_text_info, UNIFORM_VEC4, gui::Uniforms::COUNT, "u_atlas_info");

    char* test_file = load_string("../src/test/static_geometry.c");

    g_editor.set_content(test_file); // [TEST]

    g_watch.init(g_editor.state.buffer.data, g_editor.state.buffer.size);

    update_script_context(test_file); // [TEST]

    unload(test_file);

    if (g_script_ctx.callbacks.setup)
    {
        // TODO : Size should already be injected by now.

        pass(0);

        g_script_ctx.callbacks.setup();
    }
}

static void update()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    {
        pass(g_gui.resources.pass_gui);

        g_gui.begin_frame();

        g_editor.update(g_gui, ID);

        g_gui.end_frame();
    }

    if (g_script_ctx.callbacks.update)
    {
        ScriptContext*ctx = &g_script_ctx;

        ctx->viewport = script_viewport(g_editor);

        pass(0);

        viewport(
            static_cast<int>(ctx->viewport.x0),
            static_cast<int>(ctx->viewport.y0),
            static_cast<int>(ctx->viewport.width ()),
            static_cast<int>(ctx->viewport.height())
        );

        if (g_watch.update(g_editor.state.buffer.data, g_editor.state.buffer.size))
        {
            if (update_script_context(g_editor.state.buffer.data) && g_script_ctx.callbacks.setup)
            {
                ctx->callbacks.setup();
            }
        }

        ctx->callbacks.update();
    }
}


// -----------------------------------------------------------------------------
// MINIMO MAIN ENTRY
// -----------------------------------------------------------------------------

MNM_MAIN(nullptr, setup, update, nullptr);
