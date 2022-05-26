#include "mnm_rwr_lib.cpp"

namespace mnm
{

namespace rwr
{

namespace ed
{

namespace
{

// -----------------------------------------------------------------------------
// LIMITS
// -----------------------------------------------------------------------------

constexpr u32 MAX_DRAW_LIST_SIZE     = 4096;

constexpr u32 MAX_COLOR_PALETTE_SIZE = 32;

constexpr u32 MAX_CLIP_STACK_SIZE    = 4;


// -----------------------------------------------------------------------------
// EDITOR WIDGET STATE
// -----------------------------------------------------------------------------

enum State : u8
{
    STATE_COLD,
    STATE_HOT,
    STATE_ACTIVE,
};


// -----------------------------------------------------------------------------
// EDITOR "COLOR MANAGEMENT"
// -----------------------------------------------------------------------------

enum Color : u8
{
    COLOR_BACKGROUND,
    COLOR_DIVIDER_COLD,
    COLOR_DIVIDER_HOT,
    COLOR_DIVIDER_ACTIVE,
    COLOR_LINE_NUMBER,
    COLOR_LINE_NUMBER_SELECTED,
    COLOR_STATUS_BAR,
    COLOR_TEXT,
    COLOR_TEXT_SELECTED,
};

using ColorPalette = FixedArray<Vec4, MAX_COLOR_PALETTE_SIZE>;

void set_color(ColorPalette& palette, Color key, u32 rgba)
{
    Vec4& color = palette[key];

    constexpr float mul = 1.0f / 255.0f;

    color.R = ((rgba & 0xff000000) >> 24) * mul;
    color.G = ((rgba & 0x00ff0000) >> 16) * mul;
    color.B = ((rgba & 0x0000ff00) >>  8) * mul;
    color.A = ((rgba & 0x000000ff)      ) * mul;
}


// -----------------------------------------------------------------------------
// RECTANGLE REGION
// -----------------------------------------------------------------------------

struct Rect
{
    float x0;
    float y0;
    float x1;
    float y1;
};

bool operator==(const Rect& first, const Rect& second)
{
    return
        first.x0 == second.x0 &&
        first.y0 == second.y0 &&
        first.x1 == second.x1 &&
        first.y1 == second.y1 ;
}


// -----------------------------------------------------------------------------
// EDITOR GUI ID STACK
// -----------------------------------------------------------------------------

union IdStack
{
    struct
    {
        u8  size;
        u8  data[7];
    };

    u64     hash = 0;
};

u8 top(const IdStack& stack)
{
    ASSERT(stack.size > 0, "ID stack empty.");

    return stack.data[stack.size - 1];
}

u8 pop(IdStack& stack)
{
    const u8 value = top(stack);

    // NOTE : We must explicitly clear the popped value, so that the hash is
    //        consistent.
    stack.data[stack.size-- - 1] = 0;

    return value;
}

void push(IdStack& stack, u8 id)
{
    ASSERT(stack.size < BX_COUNTOF(stack.data), "ID stack full.");

    stack.data[stack.size++] = id;
}

IdStack copy_and_push(const IdStack& stack, u8 id)
{
    IdStack copy = stack;
    push(copy, id);

    return copy;
}


// -----------------------------------------------------------------------------
// EDITOR GUI CLIP STACK
// -----------------------------------------------------------------------------

// Very limited clip stack. Can only host `MAX_CLIP_STACK_SIZE` unique values
// after being reset.
struct ClipStack
{
    FixedArray<Rect, MAX_CLIP_STACK_SIZE> rects; // Unique values, not in LIFO order!
    FixedArray<u8  , MAX_CLIP_STACK_SIZE> data;
    u8                                    size;
    u8                                    used;
};

void reset(ClipStack& stack, const Rect& viewport)
{
    stack.rects[0] = viewport;
    stack.data [0] = 0;
    stack.size     = 1;
    stack.used     = 1;
}

u8 push(ClipStack& stack, const Rect& rect)
{
    // ASSERT(size < MAX_CLIP_STACK_SIZE);

    u8 idx = U8_MAX;

    for (u8 i = 0; i < stack.used; i++)
    {
        if (stack.rects[i] == rect)
        {
            idx = i;
            break;
        }
    }

    ASSERT(
        idx != U8_MAX || stack.used < MAX_CLIP_STACK_SIZE,
        "Could not push new value."
    );

    if (idx == U8_MAX && stack.used < MAX_CLIP_STACK_SIZE)
    {
        idx = stack.used++;
        stack.rects[idx] = rect;
    }

    stack.data[stack.size++] = idx;

    return idx;
}

void pop(ClipStack& stack)
{
    ASSERT(stack.size > 0, "Clip stack empty.");

    stack.size--;
}

u8 top(const ClipStack& stack)
{
    ASSERT(stack.size > 0, "Clip stack empty.");

    return stack.data[stack.size - 1];
}


// -----------------------------------------------------------------------------
// EDITOR GRAPHIC RESOURCES
// -----------------------------------------------------------------------------

struct Resources
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
};

struct AtlasInfo
{
    float texel_size;
    float glyph_cols;
    float glyph_texel_width;
    float glyph_texel_height;
    float glyph_texel_to_screen_width_ratio;
    float glyph_texel_to_screen_hegiht_ratio;

    float _unused[2];
};

struct Uniforms
{
    AtlasInfo    atlas_info;
    ColorPalette color_palette;
    ClipStack    clip_stack; // NOTE : Must be last, since we only copy `rects`.
};

// NOTE : This is to ensure that we can safely copy instance of `Uniforms`
//        object into shader without shuffling with the layout in any way.
static_assert(
    (offsetof(Uniforms , color_palette) % sizeof(Vec4) == 0) &&
    (offsetof(Uniforms , clip_stack   ) % sizeof(Vec4) == 0) &&
    (offsetof(ClipStack, rects        )                == 0) &&
    "Invalid assumption about `Uniforms` memory layout."
);

void init(Resources& resources)
{
    // ...
}



// -----------------------------------------------------------------------------
// EDITOR FONT GLYPH CACHE
// -----------------------------------------------------------------------------

struct GlyphCache
{
    int   texture_size; // = 0;
    int   glyph_cols; //   = 0;
    float glyph_width; //  = 0.0f; // In pixels, including one-pixel padding.
    float glyph_height; // = 0.0f; // In pixels, no padding.
};

float screen_width(GlyphCache& cache)
{
    return (cache.glyph_width  - 1.0f) / dpi();
}

float screen_height(GlyphCache& cache)
{
    return cache.glyph_height / dpi();
}

u32 codepoint_index(GlyphCache& cache, int codepoint)
{
    BX_UNUSED(cache);

    if (codepoint >= 32 && codepoint <= 126)
    {
        return u32(codepoint - 32);
    }

    // TODO : Utilize hashmap for the rest of the stored characters.
    // ...

    // Replacement character.
    return 95;
}

int submit_glyph_range(u32 start, u32 end, int position_index, const GlyphCache& cache)
{
    char buffer[5] = { 0 };

    for (u32 codepoint = start; codepoint <= end; codepoint++)
    {
        const int x = position_index % cache.glyph_cols;
        const int y = position_index / cache.glyph_cols;

        position_index++;

        identity();
        translate(x * cache.glyph_width, (y + 0.25f) * cache.glyph_height, 0.0f);

        const u32 size = utf8_encode(codepoint, buffer);
        text(buffer, buffer + size);
    }

    return position_index;
}

void rebuild(GlyphCache& cache, const Resources& resources, float cap_height)
{
    ASSERT(cap_height > 0.0f, "Non-positive cap height %f.", cap_height);

    begin_atlas(
        resources.texture_tmp_atlas,
        ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE | ATLAS_ALLOW_UPDATE,
        resources.font_atlas,
        cap_height * dpi()
    );
    glyph_range(0x0020, 0x007e); // Printable ASCII.
    glyph_range(0xfffd, 0xfffd); // Replacement character.
    end_atlas();

    text_size(
        resources.texture_tmp_atlas, "X", 0, 1.0f,
        &cache.glyph_width, &cache.glyph_height
    );

    cache.glyph_width  += 1.0f;
    cache.glyph_height *= 2.0f;

    for (cache.texture_size = 128; ; cache.texture_size *= 2)
    {
        // TODO : Rounding and padding.
        cache.glyph_cols = int(cache.texture_size / cache.glyph_width );
        const int   rows = int(cache.texture_size / cache.glyph_height);

        // TODO : Check against the dynamic glyph count.
        if (cache.glyph_cols * rows >= 96)
        {
            break;
        }
    }

    begin_text(
        resources.mesh_tmp_text,
        resources.texture_tmp_atlas,
        TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT
    );
    {
        color(0xffffffff);

        int index = 0;

        index = submit_glyph_range(0x0020, 0x007e, index, cache);
        ASSERT(index == 95, "Invalid glyph cache index %i.", index);

        index = submit_glyph_range(0xfffd, 0xfffd, index, cache);
        ASSERT(index == 96, "Invalid glyph cache index %i.", index);
    }
    end_text();

    create_texture(
        resources.texture_glyph_cache,
        TEXTURE_R8 | TEXTURE_CLAMP | TEXTURE_TARGET,
        cache.texture_size,
        cache.texture_size
    );

    begin_framebuffer(resources.framebuffer_glyph_cache);
    texture(resources.texture_glyph_cache);
    end_framebuffer();

    pass(resources.pass_glyph_cache);

    framebuffer(resources.framebuffer_glyph_cache);
    clear_color(0x000000ff); // TODO : If we dynamically update the cache, we only have to clear before the first draw.
    viewport(0, 0, cache.texture_size, cache.texture_size);

    identity();
    ortho(0.0f, float(cache.texture_size), float(cache.texture_size), 0.0f, 1.0f, -1.0f);
    projection();

    identity();
    mesh(resources.mesh_tmp_text);
}


// -----------------------------------------------------------------------------
// EDITOR GUI DRAW LIST
// -----------------------------------------------------------------------------

// Simple draw list, supports only rectangles.
struct DrawList
{
    struct Header
    {
        u16 glyph_count;
        u8  color_index;
        u8  clip_index;
    };

    union Data
    {
        u32 as_u32;
        f32 as_f32;
    };

    union Item
    {
        Header header;
        Data   data;
    };

    Item data[MAX_DRAW_LIST_SIZE]; // TODO : Dynamic memory ?
    u32  size;
    u32  offset;
    u32  empty_glyph_index; // TODO : Set this when space not first in the atlas.
};

void add_rect(DrawList& list, const Rect& rect, u8 color_index, u8 clip_index)
{
    ASSERT(list.size + 5 < MAX_DRAW_LIST_SIZE, "Text editor GUI draw list full.");

    list.data[list.size++].header      = { 0, color_index, clip_index };
    list.data[list.size++].data.as_f32 = rect.x0;
    list.data[list.size++].data.as_f32 = rect.y0;
    list.data[list.size++].data.as_f32 = rect.x1;
    list.data[list.size++].data.as_f32 = rect.y1;
}

void add_glyph(DrawList& list, u32 index)
{
    ASSERT(list.size < MAX_DRAW_LIST_SIZE, "Text editor GUI draw list full.");

    list.data[list.size++].data.as_u32 = index;
}

void start_string(DrawList& list, float x, float y, u8 color_index, u8 clip_index)
{
    ASSERT(list.size + 3 < MAX_DRAW_LIST_SIZE, "Text editor GUI draw list full.");

    list.offset = list.size;

    list.data[list.size++].header      = { 0, color_index, clip_index };
    list.data[list.size++].data.as_f32 = x;
    list.data[list.size++].data.as_f32 = y;
}

void end_string(DrawList& list)
{
    const u32 glyph_count = list.size - list.offset - 3;
    ASSERT(glyph_count <= U16_MAX, "Text editor GUI string too long.");

    if (glyph_count)
    {
        list.data[list.offset].header.glyph_count = u16(glyph_count);
    }
    else
    {
        // NOTE : Empty strings (glyphs not in the cache).
        list.size -= 3;
    }
}

float encode_base_vertex(u32 glyph_index, u8 color_index, u8 clip_index)
{
    return (
        ((glyph_index  * MAX_COLOR_PALETTE_SIZE) +
          color_index) * MAX_CLIP_STACK_SIZE +
          clip_index ) * 4;
}

void submit
(
    const DrawList&  list,
    const Resources& resources,
    const Vec2&      glyph_size,
    const void*      uniforms
)
{
    if (list.size == 0)
    {
        return;
    }

    ASSERT(list.size >= 4, "Invalid list size %" PRIu32 ".", list.size);

    begin_mesh(
        resources.mesh_gui_text,
        MESH_TRANSIENT | PRIMITIVE_QUADS | NO_VERTEX_TRANSFORM
    );

    const float width  = glyph_size.X;
    const float height = glyph_size.Y;

    for (u32 i = 0; i < list.size;)
    {
        const DrawList::Header header = list.data[i++].header;

        if (header.glyph_count)
        {
            float        x0 = list.data[i++].data.as_f32;
            const float  y0 = list.data[i++].data.as_f32;
            float        x1 = x0 + width;
            const float  y1 = y0 + height;

            for (u32 j = 0; j < header.glyph_count; j++, i++)
            {
                const float vtx = encode_base_vertex(
                    list.data[i].data.as_u32,
                    header.color_index,
                    header.clip_index
                );

                vertex(x0, y0, vtx + 0.0f);
                vertex(x0, y1, vtx + 1.0f);
                vertex(x1, y1, vtx + 2.0f);
                vertex(x1, y0, vtx + 3.0f);

                x0  = x1;
                x1 += width;
            }
        }
        else
        {
            const float x0  = list.data[i++].data.as_f32;
            const float y0  = list.data[i++].data.as_f32;
            const float x1  = list.data[i++].data.as_f32;
            const float y1  = list.data[i++].data.as_f32;

            const float vtx = encode_base_vertex(
                list.empty_glyph_index,
                header.color_index, 
                header.clip_index
            );

            vertex(x0, y0, vtx + 0.0f);
            vertex(x0, y1, vtx + 1.0f);
            vertex(x1, y1, vtx + 2.0f);
            vertex(x1, y0, vtx + 3.0f);
        }
    }

    end_mesh();

    identity();
    state   (STATE_BLEND_ALPHA | STATE_WRITE_RGB);
    uniform (resources.uniform_text_info, uniforms);
    texture (resources.texture_glyph_cache);
    shader  (resources.program_gui_text);
    mesh    (resources.mesh_gui_text);

    // list.offset = 0;
}


// -----------------------------------------------------------------------------
// EDITOR CALLBACKS
// -----------------------------------------------------------------------------

void init(void)
{
    // ...
}

void setup(void)
{
    // ...
}

void draw(void)
{
    // ...
}

void cleanup(void)
{
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
