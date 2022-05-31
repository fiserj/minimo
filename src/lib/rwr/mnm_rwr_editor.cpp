#include "mnm_rwr_lib.cpp"

#include <imgui.h>

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

void ImGui_Impl_BeginFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    if (!io.BackendRendererUserData || dpi_changed())
    {
        unsigned char* data;
        int            width, height;

        io.FontGlobalScale         = 1.0f / dpi();
        io.DisplayFramebufferScale = { 1.0f, 1.0f };

        ImGui::GetStyle().ScaleAllSizes(1.0f / dpi());

        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF("/Users/fiser/Downloads/segoe-ui-4-cufonfonts/Segoe UI.ttf", 18.0f * dpi());
        io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

        load_texture(IMGUI_FONT_TEXTURE_ID, TEXTURE_R8, width, height, width, data);

        io.BackendRendererUserData = reinterpret_cast<void*>(true);
    }

    io.DeltaTime   = float(dt());
    io.DisplaySize = { ::width(), ::height() };
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

            // TODO : Scissors, clipping.
            identity();
            texture(IMGUI_FONT_TEXTURE_ID); // cmd->GetTexID()
            state(STATE_WRITE_RGB | STATE_WRITE_A | STATE_BLEND_ALPHA);
            mesh(mesh_id);
        }
    }

    ::pop();
}


// -----------------------------------------------------------------------------
// EDITOR CALLBACKS
// -----------------------------------------------------------------------------

void init(void)
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // ...
}

void setup(void)
{
    title("MiNiMo Source Code Editor");

    clear_color(0x333333ff);
    clear_depth(1.0f);

    // ...
}

void draw(void)
{
    if (!ImGui::GetIO().WantCaptureKeyboard && key_down(KEY_ESCAPE))
    {
        quit();
    }

    ImGui_Impl_BeginFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize({ 300.0f, 100.0f }, ImGuiCond_Always);
    ImGui::SetNextWindowPos ({  50.0f, 100.0f }, ImGuiCond_Always);

    if (ImGui::Begin("Hello, World!"))
    {
        // ...
    }
    ImGui::End();

    // ...

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
