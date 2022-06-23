#include "mnm_rwr_lib.cpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <imgui_impl_glfw.cpp>

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
        const float font_scale = get_font_scale(font_segoe_ui_data, cap_size);

        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        font_cfg.OversampleH          = 2.0f;

        io.Fonts->Clear();
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned int*>(font_segoe_ui_data),
            font_segoe_ui_size,
            font_scale,
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

    for (int i = 0, mesh_id = IMGUI_BASE_MESH_ID; i < draw_data->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[i];

        for (int j = 0; j < cmd_list->CmdBuffer.Size; j++, mesh_id--)
        {
            const ImDrawCmd* cmd = &cmd_list->CmdBuffer[j];
            ASSERT(!cmd->UserCallback, "Custom ImGui render callback not supported.");

            const ImDrawVert* vertices = cmd_list->VtxBuffer.Data + cmd->VtxOffset;
            const ImDrawIdx*  indices  = cmd_list->IdxBuffer.Data + cmd->IdxOffset;

            identity();

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

            identity();
            texture(IMGUI_FONT_TEXTURE_ID); // cmd->GetTexID()
            state(STATE_WRITE_RGB | STATE_WRITE_A | STATE_BLEND_ALPHA);
            mesh(mesh_id);
        }
    }

    ::pop();
}


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

        if (ImGui::BeginViewportSideBar("Status", viewport, ImGuiDir_Down, bar_height, ImGuiWindowFlags_None))
        {
            ImGui::TextUnformatted("Status Bar Placeholder...");
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

    if (ImGui::Begin("Editor"))
    {
        // ...
    }
    ImGui::End();

    const ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dockspace_id);
    // ...
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

    // ...
}

void setup(void)
{
    vsync(1); // TODO : Leave this on the edited source code?

    title("MiNiMo Source Code Editor");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    // ...
}

void draw(void)
{
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

void cleanup(void)
{
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

    return mnm_run(
        ed::init,
        ed::setup,
        ed::draw,
        ed::cleanup
    );
}
