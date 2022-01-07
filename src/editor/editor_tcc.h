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
    TCCState*       state;
    ScriptCallbacks callbacks;
    gui::Rect       viewport;
    bool            wants_input;
    bool            quit_requested;
    char            last_error[512];
};

struct ScriptContextScope
{
    ScriptContext& ctx;

    ScriptContextScope(ScriptContext& ctx)
        : ctx(ctx)
    {
    }

    ~ScriptContextScope()
    {
        if (ctx.state)
        {
            tcc_delete(ctx.state);
        }
    }
};

static ScriptContext g_script_ctx = {};

static ScriptContextScope g_script_ctx_scope(g_script_ctx);


// -----------------------------------------------------------------------------
// INTERCEPTED MiNiMo API CALLS
// -----------------------------------------------------------------------------

static int mnm_run_intercepted(void (* init)(void), void (* setup)(void), void (* update)(void), void (* cleanup)(void))
{
    g_script_ctx.callbacks.init    = init;
    g_script_ctx.callbacks.setup   = setup;
    g_script_ctx.callbacks.update  = update;
    g_script_ctx.callbacks.cleanup = cleanup;

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
    SCRIPT_FUNC(mesh),
    SCRIPT_FUNC(ortho),
    SCRIPT_FUNC(projection),
    SCRIPT_FUNC(rotate),
    SCRIPT_FUNC(rotate_x),
    SCRIPT_FUNC(rotate_y),
    SCRIPT_FUNC(rotate_z),
    SCRIPT_FUNC(vertex),
};


// -----------------------------------------------------------------------------
// SCRIPT SOURCE UPDATE
// -----------------------------------------------------------------------------

static const float  s_tcc__mzerosf = -0.0f;

static const double s_tcc__mzerodf = -0.0;

static ScriptContext* get_script_context(const char* source)
{
    if (g_script_ctx.state)
    {
        tcc_delete(g_script_ctx.state);
        g_script_ctx.state = nullptr;
    }

    g_script_ctx.state = tcc_new();

    if (!g_script_ctx.state)
    {
        return nullptr;
    }

    tcc_set_error_func(g_script_ctx.state, nullptr, [](void*, const char* message)
    {
        strncpy(g_script_ctx.last_error, message, sizeof(g_script_ctx.last_error));
    });

    tcc_set_options(g_script_ctx.state, "-nostdinc -nostdlib");

    (void)tcc_add_include_path(g_script_ctx.state, MNM_INCLUDE_PATH);
    (void)tcc_add_include_path(g_script_ctx.state, TCC_INCLUDE_PATH);

    (void)tcc_add_symbol(g_script_ctx.state, "__mzerosf", &s_tcc__mzerosf);
    (void)tcc_add_symbol(g_script_ctx.state, "__mzerodf", &s_tcc__mzerodf);

    for (int i = 0; i < BX_COUNTOF(s_script_funcs); i++)
    {
        if (0 > tcc_add_symbol(g_script_ctx.state, s_script_funcs[i].name, s_script_funcs[i].func))
        {
            ASSERT(false && "Could not add symbol.");
            return nullptr;
        }
    }

    if (0 > tcc_set_output_type(g_script_ctx.state, TCC_OUTPUT_MEMORY))
    {
        ASSERT(false && "Could not set script output type.");
        return nullptr;
    }

    if (0 > tcc_compile_string(g_script_ctx.state, source))
    {
        ASSERT(false && "Could not compile source code.");
        return nullptr;
    }

    if (0 > tcc_relocate(g_script_ctx.state, TCC_RELOCATE_AUTO))
    {
        ASSERT(false && "Could not relocate code.");
        return nullptr;
    }

    if (auto main_func = reinterpret_cast<int (*)(int, char**)>(tcc_get_symbol(g_script_ctx.state, "main")))
    {
        // TODO ? Maybe pass the arguments given to the editor?
        const char* argv[] = { "MiNiMoEd" };
        main_func(1, const_cast<char**>(argv));
    }
    else
    {
        ASSERT(false && "Could not find 'main' symbol.");
        return nullptr;
    }

    return &g_script_ctx;
}


// -----------------------------------------------------------------------------
