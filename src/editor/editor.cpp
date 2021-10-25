#include <assert.h>                        // assert

#include <mnm/mnm.h>

#include "editor_font.h"                   // g_editor_*
#include "editor_ui.h"                     // Editor


// -----------------------------------------------------------------------------
// RESOURCE IDS
// -----------------------------------------------------------------------------

#define FONT_ID  127

#define ATLAS_ID 1023

#define TEXT_ID  4095


// -----------------------------------------------------------------------------
// GLOBAL VARIABLES
// -----------------------------------------------------------------------------

TextEdit         g_te;

TextEditSettings g_tes;


// -----------------------------------------------------------------------------
// MINIMO CALLBACKS
// -----------------------------------------------------------------------------

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x303030ff);
    clear_depth(1.0f);

    create_font(FONT_ID, g_font_data);

    // TODO : `ATLAS_ALLOW_UPDATE` seems to be broken again.
    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X, FONT_ID, g_tes.font_cap_height * dpi());
    glyph_range(0x20, 0x7e);
    end_atlas();

    // NOTE : Just a test content for now.
    set_content(g_te, load_string("../src/test/static_geometry.c"));
}

static void update()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    // NOTE : Just a test scroll for now.
    g_te.scroll_offset = (bx::cos((float)elapsed() * 0.5f + bx::kPi) * 0.5f + 0.5f) * 975.0f;

    begin_text(TEXT_ID, ATLAS_ID, TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT);
    identity();
    submit_lines(g_te, g_tes, height());
    end_text();

    identity();
    ortho(0.0f, width(), height(), 0.0f, 1.0f, -1.0f);
    projection();

    identity();
    translate(10.0f, 10.0f, 0.0f);
    mesh(TEXT_ID);
}


// -----------------------------------------------------------------------------
// MINIMO MAIN ENTRY
// -----------------------------------------------------------------------------

MNM_MAIN(nullptr, setup, update, nullptr);
