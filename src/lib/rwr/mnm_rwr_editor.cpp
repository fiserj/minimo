#include "mnm_rwr_lib.cpp"

#include <imgui.h>

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

    io.AddMousePosEvent  ( ::mouse_x(),  ::mouse_y());
    io.AddMouseWheelEvent(::scroll_x(), ::scroll_y());

    {
        constexpr int mouse_buttons[3][2] =
        {
            { MOUSE_LEFT  , ImGuiMouseButton_Left   },
            { MOUSE_MIDDLE, ImGuiMouseButton_Middle },
            { MOUSE_RIGHT , ImGuiMouseButton_Right  },
        };

        for (u32 i = 0; i < BX_COUNTOF(mouse_buttons); i++)
        {
            if (::mouse_down(mouse_buttons[i][0]))
            {
                io.AddMouseButtonEvent(mouse_buttons[i][1], true);
            }

            if (::mouse_up(mouse_buttons[i][0]))
            {
                io.AddMouseButtonEvent(mouse_buttons[i][1], false);
            }
        }
    }

    {
        // TODO : Figure out cursor handling that doesn't disrupt user's cursor logic.
        const ImGuiMouseCursor cursor = io.MouseDrawCursor
            ? ImGuiMouseCursor_None
            : ImGui::GetMouseCursor();

        constexpr int cursor_icons[] =
        {
            CURSOR_HIDDEN,      // ImGuiMouseCursor_None
            CURSOR_ARROW,       // ImGuiMouseCursor_Arrow
            CURSOR_I_BEAM,      // ImGuiMouseCursor_TextInput
            CURSOR_ARROW,       // ImGuiMouseCursor_ResizeAll
            CURSOR_RESIZE_NS,   // ImGuiMouseCursor_ResizeNS
            CURSOR_RESIZE_EW,   // ImGuiMouseCursor_ResizeEW
            CURSOR_RESIZE_NESW, // ImGuiMouseCursor_ResizeNESW
            CURSOR_RESIZE_NWSE, // ImGuiMouseCursor_ResizeNWSE
            GLFW_HAND_CURSOR,   // ImGuiMouseCursor_Hand
            CURSOR_NOT_ALLOWED, // ImGuiMouseCursor_NotAllowed
        };

        ::cursor(cursor_icons[cursor + 1]);
    }
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

    ImGui::SetNextWindowSize({ 300.0f, 100.0f }, ImGuiCond_Once);
    ImGui::SetNextWindowPos ({  50.0f, 100.0f }, ImGuiCond_Once);

    if (ImGui::Begin("Hello, World!"))
    {
        // ...

        const char* items[] = { "Apple", "Banana", "Cherry", "Kiwi", "Mango", "Orange", "Pineapple", "Strawberry", "Watermelon" };
            static int item_current = 1;
            ImGui::ListBox("listbox", &item_current, items, IM_ARRAYSIZE(items), 4);
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
