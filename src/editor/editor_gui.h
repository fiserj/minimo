#pragma once

#include <stdint.h>    // uint*_t
#include <string.h>    // memset

#ifndef ASSERT
#   include <assert.h> // assert
#   define ASSERT(cond) assert(cond)
#endif

#include <utf8.h>      // utf8codepoint, utf8size_lazy

#include <mnm/mnm.h>

#ifdef MNM_GUI_STATIC
#   define MNM_GUI_API static
#else
#   define MNM_GUI_API
#endif // MNM_GUI_STATIC

#ifndef MNM_GUI_MAX_TEXT_BUFFER_SIZE
#   define MNM_GUI_MAX_TEXT_BUFFER_SIZE 4096
#endif // MNM_GUI_MAX_TEXT_BUFFER_SIZE

#ifndef MNM_GUI_MAX_RECT_BUFFER_SIZE
#   define MNM_GUI_MAX_RECT_BUFFER_SIZE 512
#endif // MNM_GUI_MAX_RECT_BUFFER_SIZE

#ifndef MNM_GUI_UNLIKELY
#   ifdef BX_UNLIKELY
#      define MNM_GUI_UNLIKELY(cond) BX_UNLIKELY(cond)
#   else
#      define MNM_GUI_UNLIKELY(cond) (cond)
#   endif
#endif // MNM_GUI_UNLIKELY

#ifndef MNM_GUI_LIKELY
#   ifdef BX_LIKELY
#      define MNM_GUI_LIKELY(cond) BX_LIKELY(cond)
#   else
#      define MNM_GUI_LIKELY(cond) (cond)
#   endif
#endif // MNM_GUI_LIKELY


// -----------------------------------------------------------------------------

typedef enum State
{
    STATE_COLD,
    STATE_HOT,
    STATE_ACTIVE,

} State;

typedef struct Rect
{
    float x0;
    float y0;
    float x1;
    float y1;

} Rect;

typedef struct ColorRect
{
    uint32_t color;
    Rect     rect;

} ColorRect;

typedef struct ByteRange
{
    uint32_t start;
    uint32_t end;

} ByteRange;

typedef union IdStack
{
    struct
    {
        uint8_t size;
        uint8_t elements[7];

    }           stack;

    uint64_t    hash;

} IdStack;

typedef union ClipStack
{
    Rect    elements[4];
    uint8_t size;

} ClipStack;

// TODO : (1) Add "unknown" glyph character.
//        (2) Add support for non-ASCII characters.
//        (3) Add support for on-demand atlas update.
typedef struct GlyphCache
{
    int   texture_size;
    int   glyph_cols;
    float glyph_width;  // In pixels.
    float glyph_height; // In pixels.

} GlyphCache;

typedef struct TextBuffer
{
    uint32_t data[MNM_GUI_MAX_TEXT_BUFFER_SIZE]; // TODO : Dynamic memory ?
    uint32_t size;

    uint32_t offset;
    uint32_t length;

} TextBuffer;

typedef struct RectBuffer
{
    ColorRect data[MNM_GUI_MAX_RECT_BUFFER_SIZE]; // TODO : Dynamic memory ?
    uint32_t  size;

} RectBuffer;

typedef struct ResourceIds
{
    int font_atlas;

    int framebuffer_glyph_cache;

    int mesh_tmp_text;
    int mesh_gui_rects;
    int mesh_gui_text;

    int pass_glyph_cache;
    int pass_gui;

    int program_gui_text;

    int texture_glyph_cache;
    int texture_tmp_atlas;

    int uniform_text_info;

} ResourceIds;

typedef struct Gui
{
    TextBuffer  text_buffer;
    RectBuffer  rect_buffer;
    IdStack     active_stack;
    IdStack     current_stack;
    ClipStack   clip_stack;
    GlyphCache  glyph_cache;
    ResourceIds ids;
    float       font_cap_height;
    int         cursor;

} Gui;


// -----------------------------------------------------------------------------

MNM_GUI_API inline float rect_width(const Rect* rect)
{
    ASSERT(rect);

    return rect->x1 - rect->x0;
}

MNM_GUI_API inline float rect_height(const Rect* rect)
{
    ASSERT(rect);

    return rect->y1 - rect->y0;
}


// -----------------------------------------------------------------------------

MNM_GUI_API inline uint8_t id_stack_top(IdStack* s)
{
    ASSERT(s);
    ASSERT(s->stack.size);

    return s->stack.elements[s->stack.size - 1];
}

MNM_GUI_API inline void id_stack_push(IdStack* s, uint8_t id)
{
    ASSERT(s);
    ASSERT(s->stack.size < 7);

    s->stack.elements[s->stack.size++] = id;
}

MNM_GUI_API inline uint8_t id_stack_pop(IdStack* s)
{
    const uint8_t value = id_stack_top(s);

    // NOTE : For hash consistency, we have to explicitly clear the popped slot.
    s->stack.elements[s->stack.size-- - 1] = 0;

    return value;
}

MNM_GUI_API inline IdStack id_stack_copy_and_push(IdStack* s, uint8_t id)
{
    ASSERT(s);

    IdStack copy = *s;
    id_stack_push(&copy, id);

    return copy;
}


// -----------------------------------------------------------------------------

MNM_GUI_API void glyph_screen_size(const GlyphCache* gc, float* out_width, float* out_height)
{
    ASSERT(gc);

    if (out_width)
    {
        *out_width = (gc->glyph_width - 1.0f) / dpi();
    }

    if (out_height)
    {
        *out_height = gc->glyph_height / dpi();
    }
}

MNM_GUI_API void glyph_cache_rebuild(GlyphCache* gc, const ResourceIds* ids, float cap_height)
{
    ASSERT(gc);

    begin_atlas(
        ids->texture_tmp_atlas,
        ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE, // TODO : `ATLAS_ALLOW_UPDATE` seems to be broken again.
        ids->font_atlas,
        cap_height * dpi()
    );
    glyph_range(0x20, 0x7e);
    end_atlas();

    text_size(ids->texture_tmp_atlas, "X", 0, 1.0f, &gc->glyph_width, &gc->glyph_height);

    gc->glyph_width  += 1.0f;
    gc->glyph_height *= 2.0f;

    for (gc->texture_size = 128; ; gc->texture_size *= 2)
    {
        // TODO : Rounding and padding.
        gc->glyph_cols = (int)(gc->texture_size / gc->glyph_width );
        const int rows = (int)(gc->texture_size / gc->glyph_height);

        if (gc->glyph_cols * rows >= 95)
        {
            break;
        }
    }

    begin_text(
        ids->mesh_tmp_text,
        ids->texture_tmp_atlas,
        TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT
    );
    {
        color(0xffffffff);

        for (int i = 0; i < 95; i++)
        {
            const int x = i % gc->glyph_cols;
            const int y = i / gc->glyph_cols;

            identity();
            translate(x * gc->glyph_width, (y + 0.25f) * gc->glyph_height, 0.0f);

            char letter[2] = { (char)(i + 32), 0 };
            text(letter, 0);
        }
    }
    end_text();

    create_texture(
        ids->texture_glyph_cache,
        TEXTURE_R8 | TEXTURE_CLAMP | TEXTURE_TARGET,
        gc->texture_size,
        gc->texture_size
    );

    begin_framebuffer(ids->framebuffer_glyph_cache);
    texture(ids->texture_glyph_cache);
    end_framebuffer();

    pass(ids->pass_glyph_cache);

    framebuffer(ids->framebuffer_glyph_cache);
    clear_color(0x000000ff); // TODO : If we dynamically update the cache, we only have to clear before the first draw.
    viewport(0, 0, gc->texture_size, gc->texture_size);

    identity();
    ortho(0.0f, (float)gc->texture_size, (float)gc->texture_size, 0.0f, 1.0f, -1.0f);
    projection();

    identity();
    mesh(ids->mesh_tmp_text);
}


// -----------------------------------------------------------------------------

MNM_GUI_API void text_buffer_start(TextBuffer* tb, uint32_t color, float x, float y)
{
    ASSERT(tb);

    tb->offset = tb->size++;
    tb->length = 0;

    tb->data[tb->size++] = color;
    tb->data[tb->size++] = *(uint32_t*)&x;
    tb->data[tb->size++] = *(uint32_t*)&y;
}

MNM_GUI_API inline void text_buffer_add(TextBuffer* tb, uint32_t index)
{
    ASSERT(tb);
    ASSERT(tb->size + tb->length < MNM_GUI_MAX_TEXT_BUFFER_SIZE);

    tb->data[tb->length++] = index;
}

MNM_GUI_API inline void text_buffer_end(TextBuffer* tb)
{
    ASSERT(tb);

    tb->data[tb->offset] = tb->length;
    tb->size += tb->length;
}

MNM_GUI_API void text_buffer_submit(TextBuffer* tb, const GlyphCache* gc, const ResourceIds* ids)
{
    ASSERT(tb);
    ASSERT(gc);
    ASSERT(ids);

    if (MNM_GUI_UNLIKELY(tb->size == 0))
    {
        return;
    }

    ASSERT(tb->size >= 4);

    begin_mesh(ids->mesh_gui_text, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR | NO_VERTEX_TRANSFORM);

    float width, height;
    glyph_screen_size(gc, &width, &height);

    for (uint32_t i = 0; i < tb->size;)
    {
        const uint32_t length =           tb->data[i++];
        const uint32_t color_ =           tb->data[i++];
        float          x0     = *(float*)&tb->data[i++];
        const float    y0     = *(float*)&tb->data[i++];
        float          x1     = x0 + width;
        const float    y1     = y0 + height;

        color(color_);

        for (uint32_t j = 0; j < length; j++, i++)
        {
            // TODO ? Pack also color and clip rectangle index (from a palette of, say, 16 colors) ?
            const float idx = (float)tb->data[i] * 4.0f;

            vertex(x0, y0, idx + 0.0f);
            vertex(x0, y1, idx + 1.0f);
            vertex(x1, y1, idx + 2.0f);
            vertex(x1, y0, idx + 3.0f);

            x0  = x1;
            x1 += width;
        }
    }

    end_mesh();

    const float atlas_info[4] =
    {
        1.0f / gc->texture_size,
        (float)gc->glyph_cols,
        gc->glyph_width,
        gc->glyph_height,
    };

    identity();
    state   (STATE_BLEND_ALPHA | STATE_WRITE_RGB);
    uniform (ids->uniform_text_info, atlas_info);
    texture (ids->texture_glyph_cache);
    shader  (ids->program_gui_text);
    mesh    (ids->mesh_gui_text);

    tb->offset =
    tb->length = 0;
}


// -----------------------------------------------------------------------------

MNM_GUI_API void gui_init(Gui* g, const ResourceIds* ids, float font_cap_height)
{
    ASSERT(g);
    ASSERT(ids);

    memset(g, 0, sizeof(*g));
    g->ids             = *ids;
    g->font_cap_height = font_cap_height;
}

MNM_GUI_API void gui_init_default(Gui* g)
{
    ResourceIds ids = { 0 };

    // TODO : Change when limits can be queried.
    ids.font_atlas              = 127;
    ids.framebuffer_glyph_cache = 127;
    ids.mesh_tmp_text           = 4093;
    ids.mesh_gui_rects          = 4094;
    ids.mesh_gui_text           = 4095;
    ids.pass_glyph_cache        = 62;
    ids.pass_gui                = 63;
    ids.program_gui_text        = 127;
    ids.texture_glyph_cache     = 1022;
    ids.texture_tmp_atlas       = 1023;
    ids.uniform_text_info       = 255;

    gui_init(g, &ids, 8.0f);
}

MNM_GUI_API void gui_begin_frame(Gui* g)
{
    ASSERT(g);

    if (dpi_changed())
    {
        glyph_cache_rebuild(&g->glyph_cache, &g->ids, g->font_cap_height);
    }

    g->cursor = CURSOR_ARROW;
    g->rect_buffer.size = 0;
}

MNM_GUI_API void gui_end_frame(Gui* g)
{
    ASSERT(g);
    ASSERT(g->current_stack.stack.size == 0);

    cursor(g->cursor);

    if (!(mouse_down(MOUSE_LEFT) || mouse_held(MOUSE_LEFT)))
    {
        g->active_stack.hash = 0;
    }

    if (MNM_GUI_UNLIKELY( g->rect_buffer.size == 0))
    {
        return;
    }

    begin_mesh(g->ids.mesh_gui_rects, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR | NO_VERTEX_TRANSFORM);
    {
        for (uint32_t i = 0; i < g->rect_buffer.size; i++)
        {
            const ColorRect* rect = &g->rect_buffer.data[i];

            color(rect->color);

            vertex(rect->rect.x0, rect->rect.y0, 0.0f);
            vertex(rect->rect.x0, rect->rect.y1, 0.0f);
            vertex(rect->rect.x1, rect->rect.y1, 0.0f);
            vertex(rect->rect.x1, rect->rect.y0, 0.0f);
        }
    }
    end_mesh();

    identity();
    state(STATE_WRITE_RGB);
    mesh(g->ids.mesh_gui_rects);
}


// -----------------------------------------------------------------------------

MNM_GUI_API inline void gui_rect(Gui* g, uint32_t color, Rect rect)
{
    ASSERT(g);

    g->rect_buffer.data[g->rect_buffer.size++] = (ColorRect) { color, rect };
}

MNM_GUI_API inline void gui_hline(Gui* g, uint32_t color, float y, float x0, float x1, float thickness)
{
    gui_rect(g, color, (Rect) { x0, y, x1, y + thickness });
}

MNM_GUI_API inline void gui_vline(Gui* g, uint32_t color, float x, float y0, float y1, float thickness)
{
    gui_rect(g, color, (Rect) { x, y0, x + thickness, y1 });
}

MNM_GUI_API void gui_text(Gui* g, const char* string, uint32_t color, float x, float y)
{
    ASSERT(g);

    text_buffer_start(&g->text_buffer, color, x, y);

    utf8_int32_t codepoint = 0;

    for (void* it = utf8codepoint(string, &codepoint); codepoint; it = utf8codepoint(it, &codepoint))
    {
        // TODO : The codepoint-to-index should be handled by the glyph cache.
        if (MNM_GUI_LIKELY(codepoint >= 32 && codepoint <= 126))
        {
            text_buffer_add(&g->text_buffer, codepoint - 32);
        }
    }

    text_buffer_end(&g->text_buffer);
}

MNM_GUI_API void gui_text_ex(Gui* g, const char* start, const char* end, uint32_t max_chars, uint32_t color, float x, float y)
{
    ASSERT(g);

    if (start != end)
    {
        text_buffer_start(&g->text_buffer, color, x, y);

        utf8_int32_t codepoint = 0;
        uint32_t     i         = 0;

        for (void* it = utf8codepoint(start, &codepoint); it != end && i < max_chars; it = utf8codepoint(it, &codepoint), i++)
        {
            // TODO : The codepoint-to-index should be handled by the glyph cache.
            if (MNM_GUI_LIKELY(codepoint >= 32 && codepoint <= 126))
            {
                text_buffer_add(&g->text_buffer, codepoint - 32);
            }
        }

        text_buffer_end(&g->text_buffer);
    }
}


// -----------------------------------------------------------------------------
