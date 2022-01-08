#pragma once

#include <tinycc/libtcc.h> // tcc_*


// TODO : The headers should just be embedded into the binary.
#ifndef MNM_INCLUDE_PATH
#   error Define `MNM_INCLUDE_PATH` path.
#endif
#ifndef TCC_INCLUDE_PATH
#   error Define `TCC_INCLUDE_PATH` path.
#endif


// -----------------------------------------------------------------------------
// SCRIPT CONTEXT
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
    TCCState*       tcc_state       = nullptr;
    ScriptCallbacks callbacks       = {};
    gui::Rect       viewport        = {};
    bool            wants_input     = false;
    bool            quit_requested  = false;
    char            last_error[512] = { 0 };

    ~ScriptContext()
    {
        if (tcc_state)
        {
            tcc_delete(tcc_state);
            tcc_state = nullptr;
        }
    }

    ScriptContext() = default;

    ScriptContext(const ScriptContext&) = delete;

    ScriptContext& operator=(const ScriptContext&) = delete;
};

static ScriptCallbacks g_tmp_callbacks;

static ScriptContext g_script_ctx;


// -----------------------------------------------------------------------------
// INTERCEPTED MiNiMo API CALLS
// -----------------------------------------------------------------------------

static int mnm_run_intercepted(void (* init)(void), void (* setup)(void), void (* update)(void), void (* cleanup)(void))
{
    g_tmp_callbacks.init    = init;
    g_tmp_callbacks.setup   = setup;
    g_tmp_callbacks.update  = update;
    g_tmp_callbacks.cleanup = cleanup;

    return 0;
}

static void title_intercepted(const char* title)
{
    char buf[128] = { 0 };

    strcat(buf, title);
    strcat(buf, " | MiNiMo Editor");

    ::title(buf);
}

static int pixel_width_intercepted(void)
{
    return static_cast<int>(g_script_ctx.viewport.width());
}

static int pixel_height_intercepted(void)
{
    return static_cast<int>(g_script_ctx.viewport.height());
}

static float aspect_intercepted(void)
{
    return static_cast<float>(pixel_width_intercepted()) / static_cast<float>(pixel_height_intercepted());
}

static void quit_intercepted(void)
{
    g_script_ctx.quit_requested = true;
}

static int key_down_intercepted(int key)
{
    return g_script_ctx.wants_input ? key_down(key) : 0;
}

static void viewport_intercepted(int x, int y, int width, int height)
{
    // TODO : We'll have to cache and handle the symbolic constants ourselves (also for textures).
    viewport(x, y, width, height);
}


// -----------------------------------------------------------------------------
// EXPOSED FUNCTIONS' TABLE
// -----------------------------------------------------------------------------

#define SCRIPT_FUNC(name) { #name, reinterpret_cast<const void*>(name) }

#define SCRIPT_FUNC_INTERCEPTED(name) { #name, reinterpret_cast<const void*>(name##_intercepted) }

struct ScriptFunc
{
    const char* name;
    const void* func;
};

static const ScriptFunc s_script_funcs[] =
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

static const float  s_tcc__mzerosf = -0.0f;

static const double s_tcc__mzerodf = -0.0;

static TCCState* create_tcc_state(const char* source)
{
    ASSERT(source);

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

    for (int i = 0; i < BX_COUNTOF(s_script_funcs); i++)
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

static ScriptCallbacks get_script_callbacks(TCCState* tcc_state)
{
    if (auto main_func = reinterpret_cast<int (*)(int, char**)>(tcc_get_symbol(tcc_state, "main")))
    {
        // TODO ? Maybe pass the arguments given to the editor?
        const char* argv[] = { "MiNiMoEd" };
        main_func(1, const_cast<char**>(argv));

        return g_tmp_callbacks;
    }

    strcpy(g_script_ctx.last_error, "Could not find 'main' symbol.");

    return {};
}

static bool update_script_context(const char* source)
{
    bool success = false;

    if (TCCState* tcc_state = create_tcc_state(source))
    {
        ScriptCallbacks callbacks = get_script_callbacks(tcc_state);

        if (callbacks.update)
        {
            std::swap(g_script_ctx.tcc_state, tcc_state);

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
