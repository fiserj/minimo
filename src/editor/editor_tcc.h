#pragma once

#include <memory>   // unique_ptr

#include <libtcc.h> // tcc_*

// TODO : The header should just be embedded into the binary.
#ifndef MNM_INCLUDE_PATH
#   error Define `MNM_INCLUDE_PATH` path.
#endif

#define SCRIPT_FUNC(name) { #name, reinterpret_cast<const void*>(name) }

#define EMPTY_CONTEXT { nullptr, tcc_delete }

using ScriptContext = std::unique_ptr<TCCState, void(*)(TCCState*)>;

struct ScriptFunc
{
    const char* name;
    const void* func;
};

static const ScriptFunc s_script_funcs[] =
{
    SCRIPT_FUNC(mouse_x),
    SCRIPT_FUNC(mouse_y),
};

static ScriptContext create_tcc_context()
{
    ScriptContext ctx(tcc_new(), tcc_delete);
    ASSERT(ctx);

    tcc_set_options(ctx.get(), "-nostdinc -nostdlib");

    (void)tcc_add_include_path(ctx.get(), MNM_INCLUDE_PATH);

    for (int i = 0; i < BX_COUNTOF(s_script_funcs); i++)
    {
        if (0 > tcc_add_symbol(ctx.get(), s_script_funcs[i].name, s_script_funcs[i].func))
        {
            assert(false && "Could not add symbol.");
            return EMPTY_CONTEXT;
        }
    }

    if (0 > tcc_set_output_type(ctx.get(), TCC_OUTPUT_MEMORY))
    {
        assert(false && "Could not set script output type.");
        return EMPTY_CONTEXT;
    }

    return ctx;
}
