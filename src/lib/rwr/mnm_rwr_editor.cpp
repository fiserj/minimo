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
    int font_atlas              = 0;

    int framebuffer_glyph_cache = 0;

    int mesh_tmp_text           = 0;
    int mesh_gui_rects          = 0;
    int mesh_gui_text           = 0;

    int pass_glyph_cache        = 0;
    int pass_gui                = 0;

    int program_gui_text        = 0;

    int texture_glyph_cache     = 0;
    int texture_tmp_atlas       = 0;

    int uniform_text_info       = 0;
};

void init(Resources& resources)
{
    // ...
}


// -----------------------------------------------------------------------------
// EDITOR FONT GLYPH CACHE
// -----------------------------------------------------------------------------

struct GlyphCache
{
    int   texture_size = 0;
    int   glyph_cols   = 0;
    float glyph_width  = 0.0f; // In pixels, including one-pixel padding.
    float glyph_height = 0.0f; // In pixels, no padding.
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
    if (codepoint >= 32 && codepoint <= 126)
    {
        return u32(codepoint - 32);
    }

    // TODO : Utilize hashmap for the rest of the stored characters.
    // ...

    // Replacement character.
    return 95;
}

void rebuild(GlyphCache& cache, const Resources& resources, float cap_height)
{
    // ...
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
        u32 _u32;
        f32 _f32;
    };

    struct Item
    {
        Header header;
        Data   data;
    };

    Item data[MAX_DRAW_LIST_SIZE]; // TODO : Dynamic memory ?
    u32  size;
    u32  offset;
    u32  empty_glyph_index; // TODO : Set this when space not first in the atlas.
};


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
