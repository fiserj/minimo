#include "mnm_rwr_lib.cpp"

#include <bx/file.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <imgui_impl_glfw.cpp>

#include <TextEditor.h>

#include <tinycc/libtcc.h>

#include "font_input_mono.h"
#include "font_segoe_ui.h"

namespace mnm
{

namespace rwr
{

namespace ed
{

namespace
{

// -----------------------------------------------------------------------------
// IMGUI ADAPTERS
// -----------------------------------------------------------------------------

constexpr int IMGUI_PASS_ID         = MAX_PASSES   - 1;
constexpr int IMGUI_FONT_TEXTURE_ID = MAX_TEXTURES - 1;
constexpr int IMGUI_BASE_MESH_ID    = MAX_MESHES   - 1;

void ImGui_Impl_Init()
{
    ImGuiIO& io = ImGui::GetIO();

    io.BackendRendererUserData = nullptr;
    io.BackendRendererName     = "MiNiMo";
}

float get_font_scale(const void* ttf_data, float cap_pixel_size)
{
    stbtt_fontinfo font_info = {};
    stbtt_InitFont(&font_info, reinterpret_cast<const unsigned char*>(ttf_data), 0);

    i16 cap_height = 0;
    {
        const int table = stbtt__find_table(
            font_info.data,
            font_info.fontstart,
            "OS/2"
        );

        if (table && ttUSHORT(font_info.data + table) >= 2) // Version.
        {
            cap_height = ttSHORT(font_info.data + table + 88); // sCapHeight.
        }

        // TODO : Estimate cap height from capital `H` bounding box?
        ASSERT(cap_height, "Can't determine cap height.");
    };

    int ascent, descent;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, nullptr);

    // TODO : Fix possible division by zero.
    return (ascent - descent) * cap_pixel_size / cap_height;
}

void ImGui_Impl_BeginFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    if (!io.BackendRendererUserData || dpi_changed())
    {
        unsigned char* data;
        int            width, height;

        io.FontGlobalScale         = 0.5f / dpi(); // TODO : Investigate behavior with DPI other than 2.0.
        io.DisplayFramebufferScale = { 1.0f, 1.0f };

        const float cap_size   = bx::round(18.0f * dpi()); // TODO : Allow user to specify size.

        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        font_cfg.OversampleH          = 2.0f;

        io.Fonts->Clear();
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned int*>(font_segoe_ui_data),
            font_segoe_ui_size,
            get_font_scale(font_segoe_ui_data, cap_size),
            &font_cfg
        );
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned int*>(font_input_mono_data),
            font_input_mono_size,
            get_font_scale(font_input_mono_data, cap_size),
            &font_cfg
        );
        io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

        load_texture(IMGUI_FONT_TEXTURE_ID, TEXTURE_R8, width, height, width, data);

        io.BackendRendererUserData = reinterpret_cast<void*>(true);
    }

    io.DeltaTime   = float(dt());
    io.DisplaySize = { ::width(), ::height() };

    if (ImGui_ImplGlfw_GetBackendData()->WantUpdateMonitors)
    {
        ImGui_ImplGlfw_UpdateMonitors();
    }

    ImGui_ImplGlfw_UpdateMouseData  ();
    ImGui_ImplGlfw_UpdateMouseCursor();
    ImGui_ImplGlfw_UpdateGamepads   ();
}

void ImGui_Impl_EndFrame()
{
    const ImDrawData* draw_data = ImGui::GetDrawData();

    if (!draw_data->CmdListsCount)
    {
        return;
    }

    ::push();

    identity();
    ortho(
        draw_data->DisplayPos.x,
        draw_data->DisplayPos.x + draw_data->DisplaySize.x,
        draw_data->DisplayPos.y + draw_data->DisplaySize.y,
        draw_data->DisplayPos.y,
        -1.0f,
        +1.0f
    );
    projection();

    identity();

    for (int i = 0, mesh_id = IMGUI_BASE_MESH_ID; i < draw_data->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[i];

        for (int j = 0; j < cmd_list->CmdBuffer.Size; j++, mesh_id--)
        {
            const ImDrawCmd* cmd = &cmd_list->CmdBuffer[j];
            ASSERT(!cmd->UserCallback, "Custom ImGui render callback not supported.");

            const ImDrawVert* vertices = cmd_list->VtxBuffer.Data + cmd->VtxOffset;
            const ImDrawIdx*  indices  = cmd_list->IdxBuffer.Data + cmd->IdxOffset;

            begin_mesh(mesh_id, MESH_TRANSIENT | VERTEX_COLOR | VERTEX_TEXCOORD | NO_VERTEX_TRANSFORM);

            for (unsigned int k = 0; k < cmd->ElemCount; k++)
            {
                const ImDrawVert& vertex = vertices[indices[k]];

                color   (bx::endianSwap(vertex.col));
                texcoord(vertex.uv.x, vertex.uv.y);
                ::vertex(vertex.pos.x, vertex.pos.y, 0.0f);
            }

            end_mesh();

            // TODO : Check the DPI handling in this is OK.
            scissor(
                int(bx::round(dpi() *  cmd->ClipRect.x)),
                int(bx::round(dpi() *  cmd->ClipRect.y)),
                int(bx::round(dpi() * (cmd->ClipRect.z - cmd->ClipRect.x))),
                int(bx::round(dpi() * (cmd->ClipRect.w - cmd->ClipRect.y)))
            );

            // identity();
            texture(IMGUI_FONT_TEXTURE_ID); // cmd->GetTexID()
            state(STATE_WRITE_RGB | STATE_WRITE_A | STATE_BLEND_ALPHA);
            mesh(mesh_id);
        }
    }

    ::pop();
}


// -----------------------------------------------------------------------------
// TCC SCRIPT CONTEXT
// -----------------------------------------------------------------------------

struct ScriptCallbacks
{
    void (* init   )(void);
    void (* setup  )(void);
    void (* update )(void);
    void (* cleanup)(void);
};

struct ScriptContext
{
    TCCState*       tcc_state;
    ScriptCallbacks callbacks;
    int             viewport[4]; // x, y, w, h
    bool            can_have_input;
    bool            quit_requested;
    char            last_error[512];
    char            status_msg[512];
};

void destroy(ScriptContext& ctx)
{
    if (ctx.tcc_state)
    {
        tcc_delete(ctx.tcc_state);
        ctx.tcc_state = nullptr;
    }
}

ScriptCallbacks g_tmp_callbacks = {};

ScriptContext g_script_ctx = {};


// -----------------------------------------------------------------------------
// INTERCEPTED MiNiMo API CALLS
// -----------------------------------------------------------------------------

// NOTE : This should only be run in an exclusive section.
int mnm_run_intercepted(void (* init)(void), void (* setup)(void), void (* update)(void), void (* cleanup)(void))
{
    g_tmp_callbacks.init    = init;
    g_tmp_callbacks.setup   = setup;
    g_tmp_callbacks.update  = update;
    g_tmp_callbacks.cleanup = cleanup;

    return 0;
}

void title_intercepted(const char* title)
{
    char buf[128] = { 0 };

    strcat(buf, title);
    strcat(buf, " | MiNiMo Editor");

    ::title(buf);
}

int pixel_width_intercepted(void)
{
    return g_script_ctx.viewport[2];
}

int pixel_height_intercepted(void)
{
    return g_script_ctx.viewport[3];
}

float aspect_intercepted(void)
{
    return float(pixel_width_intercepted()) / float(pixel_height_intercepted());
}

void quit_intercepted(void)
{
    g_script_ctx.quit_requested = true;
}

int key_down_intercepted(int key)
{
    return g_script_ctx.can_have_input ? key_down(key) : 0;
}

void viewport_intercepted(int x, int y, int width, int height)
{
    // TODO : We'll have to cache and handle the symbolic constants ourselves (also for textures).
    viewport(x, y, width, height);
}


// -----------------------------------------------------------------------------
// EXPOSED FUNCTIONS' TABLE
// -----------------------------------------------------------------------------

#define SCRIPT_FUNC(name) { #name, reinterpret_cast<const void*>(::name) }

#define SCRIPT_FUNC_INTERCEPTED(name) { #name, reinterpret_cast<const void*>(name##_intercepted) }

struct ScriptFunc
{
    const char* name;
    const void* func;
};

const ScriptFunc s_script_funcs[] =
{
    SCRIPT_FUNC_INTERCEPTED(mnm_run),

    SCRIPT_FUNC_INTERCEPTED(aspect),
    SCRIPT_FUNC_INTERCEPTED(key_down),
    SCRIPT_FUNC_INTERCEPTED(pixel_height),
    SCRIPT_FUNC_INTERCEPTED(pixel_width),
    SCRIPT_FUNC_INTERCEPTED(quit),
    SCRIPT_FUNC_INTERCEPTED(title),
    SCRIPT_FUNC_INTERCEPTED(viewport),

    SCRIPT_FUNC(begin_mesh),
    SCRIPT_FUNC(clear_color),
    SCRIPT_FUNC(clear_depth),
    SCRIPT_FUNC(color),
    SCRIPT_FUNC(elapsed),
    SCRIPT_FUNC(end_mesh),
    SCRIPT_FUNC(identity),
    SCRIPT_FUNC(look_at),
    SCRIPT_FUNC(mesh),
    SCRIPT_FUNC(ortho),
    SCRIPT_FUNC(perspective),
    SCRIPT_FUNC(pop),
    SCRIPT_FUNC(projection),
    SCRIPT_FUNC(push),
    SCRIPT_FUNC(rotate),
    SCRIPT_FUNC(rotate_x),
    SCRIPT_FUNC(rotate_y),
    SCRIPT_FUNC(rotate_z),
    SCRIPT_FUNC(scale),
    SCRIPT_FUNC(translate),
    SCRIPT_FUNC(vertex),
    SCRIPT_FUNC(view),
};


// -----------------------------------------------------------------------------
// SCRIPT SOURCE UPDATE
// -----------------------------------------------------------------------------

const float  s_tcc__mzerosf = -0.0f;

const double s_tcc__mzerodf = -0.0;

TCCState* create_tcc_state(const char* source)
{
    ASSERT(source, "Invalid script source pointer.");

    TCCState* state = tcc_new();

    if (!state)
    {
        strcpy(g_script_ctx.last_error, "Could not create new 'TCCState'.");
        return nullptr;
    }

    tcc_set_error_func(state, nullptr, [](void*, const char* message)
    {
        strncpy(g_script_ctx.last_error, message, sizeof(g_script_ctx.last_error));
    });

    tcc_set_options(state, "-nostdinc -nostdlib");

    (void)tcc_add_include_path(state, MNM_INCLUDE_PATH);
    (void)tcc_add_include_path(state, TCC_INCLUDE_PATH);

    (void)tcc_add_symbol(state, "__mzerosf", &s_tcc__mzerosf);
    (void)tcc_add_symbol(state, "__mzerodf", &s_tcc__mzerodf);

    for (u32 i = 0; i < BX_COUNTOF(s_script_funcs); i++)
    {
        if (0 > tcc_add_symbol(state, s_script_funcs[i].name, s_script_funcs[i].func))
        {
            tcc_delete(state);
            return nullptr;
        }
    }

    if (0 > tcc_set_output_type(state, TCC_OUTPUT_MEMORY) ||
        0 > tcc_compile_string (state, source) ||
        0 > tcc_relocate       (state, TCC_RELOCATE_AUTO))
    {
        tcc_delete(state);
        return nullptr;
    }

    return state;
}

ScriptCallbacks get_script_callbacks(TCCState* tcc_state)
{
    if (auto main_func = reinterpret_cast<int (*)(int, char**)>(tcc_get_symbol(tcc_state, "main")))
    {
        static bx::Mutex mutex;
        bx::MutexScope   lock(mutex);

        // TODO ? Maybe pass the arguments given to the editor?
        const char* argv[] = { "MiNiMoEd" };
        main_func(1, const_cast<char**>(argv));

        return g_tmp_callbacks;
    }

    strcpy(g_script_ctx.last_error, "Could not find 'main' symbol.");

    return {};
}

bool update_script_context(const char* source)
{
    bool success = false;

    if (TCCState* tcc_state = create_tcc_state(source))
    {
        ScriptCallbacks callbacks = get_script_callbacks(tcc_state);

        if (callbacks.update)
        {
            bx::swap(g_script_ctx.tcc_state, tcc_state);

            g_script_ctx.callbacks      = callbacks;
            g_script_ctx.quit_requested = false;
            g_script_ctx.last_error[0]  = 0;

            success = true;
        }
        else
        {
            strcpy(g_script_ctx.last_error, "Could not determine script's 'update' function.");
        }

        if (tcc_state)
        {
            tcc_delete(tcc_state);
        }
    }

    return success;
}


// -----------------------------------------------------------------------------
// EDITOR CONTEXT
// -----------------------------------------------------------------------------

struct EditorContext
{
    // ScriptContext runtime;
    TextEditor    editor;
    bool          changed;
};

void init(EditorContext& ctx, const char* file_path)
{
    ctx = {};

    bx::FileReader reader;
    if (!bx::open(&reader, file_path))
    {
        // TODO : Signal error.
        return;
    }

    defer(bx::close(&reader));

    const i64 size = bx::getSize(&reader);

    // TODO : Use own allocator.
    char* content = reinterpret_cast<char*>(malloc(size + 1));
    defer(free(content));

    bx::read(&reader, content, i32(size), {});
    content[size] = 0;

    ctx.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
    ctx.editor.SetPalette(TextEditor::GetDarkPalette());
    ctx.editor.SetText(content);
}

EditorContext g_ed_ctx;


// -----------------------------------------------------------------------------
// EDITOR GUI
// -----------------------------------------------------------------------------

void editor_gui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    {
        const float bar_height = bx::round(ImGui::GetFontSize() * 2.0f);

        constexpr ImGuiWindowFlags bar_flags =
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoScrollWithMouse ;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

        if (ImGui::BeginViewportSideBar("Status", viewport, ImGuiDir_Down, bar_height, bar_flags))
        {
            ImGui::TextUnformatted(g_script_ctx.status_msg);
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0,0,0,0));

    const ImGuiID dockspace_id   = ImGui::GetID("EditorDockSpace");
    const bool    dockspace_init = ImGui::DockBuilderGetNode(dockspace_id);

    // Like `ImGui::DockSpaceOverViewport`, but we need to know the ID upfront.
    {
        ImGui::SetNextWindowPos     (viewport->WorkPos );
        ImGui::SetNextWindowSize    (viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoBackground          |
            ImGuiWindowFlags_NoBringToFrontOnFocus | 
            ImGuiWindowFlags_NoCollapse            |
            ImGuiWindowFlags_NoDocking             |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoNavFocus            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoTitleBar            ;

        constexpr ImGuiDockNodeFlags dockspace_flags =
            ImGuiDockNodeFlags_NoTabBar            |
            ImGuiDockNodeFlags_PassthruCentralNode ;

        char label[32];
        bx::snprintf(label, sizeof(label), "Viewport_%016x", viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding  , 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding   , { 0.0f, 0.0f });

        ImGui::Begin(label, nullptr, window_flags);

        ImGui::PopStyleVar(3);

        ImGui::DockSpace(dockspace_id, {}, dockspace_flags);

        ImGui::End();
    }

    ImGui::PopStyleColor();

    if (!dockspace_init)
    {
        ImGui::DockBuilderRemoveNode (dockspace_id);
        ImGui::DockBuilderAddNode    (dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        const ImGuiID dock_editor_id = ImGui::DockBuilderSplitNode(
            dockspace_id, ImGuiDir_Right, 0.50f, nullptr, nullptr
        );

        ImGui::DockBuilderDockWindow("Editor", dock_editor_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
    const bool editor_open = ImGui::Begin("Editor");
    ImGui::PopStyleVar();

    if (editor_open)
    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        g_ed_ctx.editor.Render("##Editor", ImGui::GetContentRegionAvail(), false);
        ImGui::PopFont();
    }
    ImGui::End();

    if (const ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dockspace_id))
    {
        g_script_ctx.viewport[0] = int(bx::round(dpi() *  node->Pos .x));
        g_script_ctx.viewport[1] = int(bx::round(dpi() *  node->Pos .y));
        g_script_ctx.viewport[2] = int(bx::round(dpi() *  node->Size.x));
        g_script_ctx.viewport[3] = int(bx::round(dpi() *  node->Size.y));
    }

    {
        static bool show_metrics_window = false;

        if (ImGui::IsKeyPressed(ImGuiKey_F11))
        {
            show_metrics_window = !show_metrics_window;
        }

        if (show_metrics_window)
        {
            ImGui::ShowMetricsWindow();
        }
    }
}


// -----------------------------------------------------------------------------
// EDITOR CALLBACKS
// -----------------------------------------------------------------------------

void init(void)
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename  = nullptr;

    // TODO : Avoid extra copy.
    const std::string& content = g_ed_ctx.editor.GetText();
    if (!content.empty())
    {
        if (update_script_context(content.c_str()) && g_script_ctx.callbacks.setup)
        {
            bx::snprintf(g_script_ctx.status_msg, sizeof(g_script_ctx.status_msg), "%.1f | Success\n", 0.0f);
        }
        else
        {
            bx::snprintf(g_script_ctx.status_msg, sizeof(g_script_ctx.status_msg), "%.1f. | Failure | %s\n", 0.0f, g_script_ctx.last_error);
        }
    }

    if (g_script_ctx.callbacks.init)
    {
        g_script_ctx.callbacks.init();
    }

    // ...
}

void setup(void)
{
    ImGui_ImplGlfw_InitForOther(g_ctx->window_handle, true);

    vsync(1); // TODO : Leave this on the edited source code?

    title("MiNiMo Source Code Editor");

    if (g_script_ctx.callbacks.setup)
    {
        // TODO : Size should already be injected by now.

        g_script_ctx.callbacks.setup();
    }
}

void update(void)
{
    // GUI pass.
    {
        pass(IMGUI_PASS_ID);

        ImGuiIO& io = ImGui::GetIO();

        if (!io.WantCaptureKeyboard && !io.WantCaptureMouse && key_down(KEY_ESCAPE))
        {
            quit();
        }

        ImGui_Impl_BeginFrame();
        ImGui::NewFrame();

        editor_gui();

        ImGui::Render();
        ImGui_Impl_EndFrame();
    }

    // Content pass.
    if (g_script_ctx.callbacks.update)
    {
        ScriptContext*ctx = &g_script_ctx;

        // TODO : This needs to be whatever pass is left set by the script.
        pass(0);

        viewport(ctx->viewport[0], ctx->viewport[1], ctx->viewport[2], ctx->viewport[3]);

        if (g_ed_ctx.editor.IsTextChanged())
        {
            g_ed_ctx.changed = true;
        }

        if (g_ed_ctx.changed && g_ed_ctx.editor.GetTimeSinceLastTextChange() > 0.75f)
        {
            g_ed_ctx.changed = false;

            const std::string& content = g_ed_ctx.editor.GetText();

            if (!content.empty() && update_script_context(content.c_str()) && g_script_ctx.callbacks.setup)
            {
                ctx->callbacks.setup();
                bx::snprintf(g_script_ctx.status_msg, sizeof(g_script_ctx.status_msg), "%.1f | Success\n", elapsed());
            }
            else
            {
                bx::snprintf(g_script_ctx.status_msg, sizeof(g_script_ctx.status_msg), "%.1f. | Failure | %s\n", elapsed(), g_script_ctx.last_error);
            }
        }

        ctx->callbacks.update();
    }
}

void cleanup(void)
{
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    // ...
}

} // unnamed namespace

} // namespace ed

} // namespace rwr

} // namespace mnm


// -----------------------------------------------------------------------------
// MAIN EDITOR ENTRY
// -----------------------------------------------------------------------------

int main(int, char**)
{
    // TODO : Argument processing (file to open, etc.).

    const char* file_path = "../src/test/hello_triangle.c";

    init(mnm::rwr::ed::g_ed_ctx, file_path);

    return mnm_run(
        ed::init,
        ed::setup,
        ed::update,
        ed::cleanup
    );
}
