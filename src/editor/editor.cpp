#include <assert.h>                        // assert

#include <mnm/mnm.h>

#include "editor_ui.h"                     // Editor


// -----------------------------------------------------------------------------
// MINIMO CALLBACKS
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// MINIMO CALLBACKS
// -----------------------------------------------------------------------------

static void setup()
{
    title("MiNiMo Editor");

    clear_color(0x303030ff);
    clear_depth(1.0f);
}

static void cleanup()
{
    // ...
}

static void update()
{
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
