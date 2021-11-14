#include <assert.h>               // assert
#include <stdint.h>               // uint*_t

#include <vector>                 // vector

#include <bgfx/embedded_shader.h> // BGFX_EMBEDDED_SHADER* (not really needed here, but necessary due to the included shader headers)

#include <bx/bx.h>                // BX_COUNTOF, BX_LIKELY

#include <utf8.h>                 // utf8codepoint, utf8size_lazy

#include <mnm/mnm.h>

#include <shaders/text_fs.h>      // text_fs
#include <shaders/text_vs.h>      // text_vs

#include "editor_font.h"          // g_font_*


// -----------------------------------------------------------------------------
// RESOURCE IDS
// -----------------------------------------------------------------------------

// Passes.
#define DEFAULT_PASS            63
#define GLYPH_CACHE_PASS        62

// Framebuffers.
#define GLYPH_CACHE_FRAMEBUFFER 127

// Textures.
#define TMP_TEXT_ATLAS          1023
#define GLYPH_CACHE_TEXTURE     1022

// Meshes.
#define TMP_TEXT_MESH           4095
#define GUI_RECT_MESH           4094
#define GUI_TEXT_MESH           4093

// Fonts.
#define GUI_FONT                127

// Shaders programs.
#define GUI_TEXT_SHADER         127

// Uniforms.
#define GUI_TEXT_INFO_UNIFORM   255


// -----------------------------------------------------------------------------
// UTILITY MACROS
// -----------------------------------------------------------------------------

// TODO : Better assert.
#define ASSERT(cond) assert(cond)

#define ID (__COUNTER__)


// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T>
using Vector = std::vector<T>;


// -----------------------------------------------------------------------------
// GLYPH CACHE
//
// We take advantage of the monospaced font and let `MiNiMo` render the glyphs
// into a texture where each glyph rectangle is of the same size. This is a bit
// wasteful on texture space size, but will enable us to use much simpler
// rendering logic and also simplifies all the text size and placement related
// calculations.
// -----------------------------------------------------------------------------

// TODO : (1) Add "unknown" glyph character.
//        (2) Add support for non-ASCII characters.
//        (3) Add support for on-demand atlas update.
struct GlyphCache
{
    int   texture_size = 0;
    int   glyph_cols   = 0;
    float glyph_width  = 0.0f;
    float glyph_height = 0.0f;

    inline void get_size(float& out_width, float& out_height)
    {
        out_width  = (glyph_width  - 1.0f) / dpi();
        out_height =  glyph_height         / dpi();
    }

    void rebuild(float cap_height)
    {
        // TODO : `ATLAS_ALLOW_UPDATE` seems to be broken again.
        begin_atlas(TMP_TEXT_ATLAS, ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE, GUI_FONT, cap_height * dpi());
        glyph_range(0x20, 0x7e);
        end_atlas();

        text_size(TMP_TEXT_ATLAS, "X", 0, 1.0f, &glyph_width, &glyph_height);

        glyph_width  += 1.0f;
        glyph_height *= 2.0f;

        for (texture_size = 128; ; texture_size *= 2)
        {
            // TODO : Rounding and padding.
            glyph_cols     = (int)(texture_size / glyph_width );
            const int rows = (int)(texture_size / glyph_height);

            if (glyph_cols * rows >= 95)
            {
                break;
            }
        }

        begin_text(TMP_TEXT_MESH, TMP_TEXT_ATLAS, TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT);
        {
            color(0xffffffff);

            for (uint8_t i = 0; i < 95; i++)
            {
                const uint8_t x = i % glyph_cols;
                const uint8_t y = i / glyph_cols;

                identity();
                translate(x * glyph_width, (y + 0.25f) * glyph_height, 0.0f);

                char letter[2] = { (char)(i + 32), 0 };
                text(letter, 0);
            }
        }
        end_text();

        create_texture(GLYPH_CACHE_TEXTURE, TEXTURE_R8 | TEXTURE_CLAMP | TEXTURE_TARGET, texture_size, texture_size);

        begin_framebuffer(GLYPH_CACHE_FRAMEBUFFER);
        texture(GLYPH_CACHE_TEXTURE);
        end_framebuffer();

        pass(GLYPH_CACHE_PASS);

        framebuffer(GLYPH_CACHE_FRAMEBUFFER);
        clear_color(0x000000ff); // TODO : If we dynamically update the cache, we only have to clear before the first draw.
        viewport(0, 0, texture_size, texture_size);

        identity();
        ortho(0.0f, texture_size, texture_size, 0.0f, 1.0f, -1.0f);
        projection();

        identity();
        mesh(TMP_TEXT_MESH);
    }
};

static GlyphCache g_cache;


// -----------------------------------------------------------------------------
// TEXT RECORDER
// -----------------------------------------------------------------------------

struct TextBuffer
{
    Vector<uint32_t> data;
    size_t           offset = 0;
    uint32_t         length = 0;

    // TODO : Encode glyph index and position within quad into each vertex's `Z`
    //        coordinate and resolve the texcoord in the vertex shader.
    void submit()
    {
        if (BX_UNLIKELY(data.empty()))
        {
            return;
        }

        ASSERT(data.size() >= 4);

        begin_mesh(GUI_TEXT_MESH, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR);

        identity();

        float width, height;
        g_cache.get_size(width, height);

        for (size_t i = 0; i < data.size();)
        {
            const uint32_t length =           data[i++];
            const uint32_t color  =           data[i++];
            float          x0     = *(float*)&data[i++];
            const float    y0     = *(float*)&data[i++];
            float          x1     = x0 + width;
            const float    y1     = y0 + height;

            ::color(color);

            for (uint32_t j = 0; j < length; j++, i++)
            {
                // TODO ? Pack also color (from a palette of, say, 16 colors) ?
                const float idx = (float)data[i] * 4.0f;

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
            (float)(1.0f / g_cache.texture_size),
            (float)g_cache.glyph_cols,
            g_cache.glyph_width,
            g_cache.glyph_height,
        };

        identity();
        shader(GUI_TEXT_SHADER);
        uniform(GUI_TEXT_INFO_UNIFORM, atlas_info);
        state(STATE_BLEND_ALPHA | STATE_WRITE_RGB);
        texture(GLYPH_CACHE_TEXTURE);
        mesh(GUI_TEXT_MESH);

        data.clear();
        offset = 0;
        length = 0;
    }

    void start(uint32_t color, float x, float y)
    {
        offset = data.size();
        length = 0;

        data.resize(data.size() + 4);

        data[offset + 1] = color;
        data[offset + 2] = *(uint32_t*)&x;
        data[offset + 3] = *(uint32_t*)&y;
    }

    inline void add(uint32_t index)
    {
        data.push_back(index);

        length++;
    }

    inline void end()
    {
        data[offset] = length;
    }
};

static TextBuffer g_text_buffer;


// -----------------------------------------------------------------------------
// GUI LOGIC
// -----------------------------------------------------------------------------

enum State
{
    STATE_COLD,
    STATE_HOT,
    STATE_ACTIVE,
};

struct Rect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

class IdStack
{
public:
    inline bool operator==(const IdStack& other) const
    {
        return m_hash == other.m_hash;
    }

    inline void clear()
    {
        m_hash = 0;
    }

    inline uint8_t size() const
    {
        return m_size;
    }

    inline uint8_t empty() const
    {
        return m_size == 0;
    }

    inline uint8_t top() const
    {
        ASSERT(m_size > 0);
        return m_stack[m_size - 1];
    }

    inline void push(uint8_t id)
    {
        ASSERT(m_size < 7);
        m_stack[m_size++] = id;
    }

    uint8_t pop()
    {
        const uint8_t value = top();

        // NOTE : We have to explicitly clear the popped value, so that the hash
        //        is consistent.
        m_stack[m_size-- - 1] = 0;

        return value;
    }

    IdStack copy_and_push(uint8_t id) const
    {
        IdStack copy = *this;
        copy.push(id);

        return copy;
    }

private:
    union
    {
        struct
        {
            uint8_t  m_size;
            uint8_t  m_stack[7];
        };

        uint64_t     m_hash = 0;
    };
};

static IdStack g_active_stack;

static IdStack g_current_stack;

static bool mouse_over(const Rect& rect)
{
    const float x = mouse_x();
    const float y = mouse_y();

    return x >= rect.x0 && x < rect.x1 &&
           y >= rect.y0 && y < rect.y1 ;
}

static inline bool none_active()
{
    return g_active_stack.empty();
}

static inline bool is_active(uint8_t id)
{
    return g_active_stack == g_current_stack.copy_and_push(id);
}

static inline void make_active(uint8_t id)
{
    g_active_stack = g_current_stack.copy_and_push(id);
}

static bool button_logic(uint8_t id, const Rect& rect, bool enabled, State& out_state)
{
    out_state = STATE_COLD;

    if (enabled && mouse_over(rect) && none_active())
    {
        out_state = STATE_HOT;

        if (mouse_down(MOUSE_LEFT))
        {
            make_active(id);
        }
    }

    if (is_active(id))
    {
        out_state = STATE_ACTIVE;
    }

    return mouse_up(MOUSE_LEFT) && is_active(id) && mouse_over(rect);
}

static bool drag_logic(uint8_t id, const Rect& rect, State& out_state, float& out_x, float& out_y)
{
    static float start_x;
    static float start_y;

    out_state = STATE_COLD;

    if (mouse_over(rect) && none_active())
    {
        out_state = STATE_HOT;

        if (mouse_down(MOUSE_LEFT))
        {
            make_active(id);

            start_x = out_x - mouse_x();
            start_y = out_y - mouse_y();
        }
    }

    if ((mouse_held(MOUSE_LEFT) || mouse_down(MOUSE_LEFT)) && is_active(id))
    {
        out_state = STATE_ACTIVE;

        out_x = start_x + mouse_x();
        out_y = start_y + mouse_y();
    }

    return out_state != STATE_COLD;
}


// -----------------------------------------------------------------------------
// GUI RENDERING
// -----------------------------------------------------------------------------

struct ColorRect
{
    uint32_t color = 0x00000000;
    Rect     rect;
};

static Vector<ColorRect> g_color_rect_list;

static inline void rect(uint32_t color, const Rect& rect)
{
    g_color_rect_list.push_back({ color, rect });
}

static inline void rect(uint32_t color, float x, float y, float width, float height)
{
    ::rect(color, { x, y, x + width, y + height });
}

static inline void hline(uint32_t color, float y, float x0, float x1, float thickness = 1.0f)
{
    // TODO : We could center it around the given `y`, but then we'd need to
    //        handle DPI here explicitly.
    rect(color, x0, y, x1 - x0, thickness);
}

static inline void vline(uint32_t color, float x, float y0, float y1, float thickness = 1.0f)
{
    // TODO : We could center it around the given `x`, but then we'd need to
    //        handle DPI here explicitly.
    rect(color, x, y0, thickness, y1 - y0);
}

// Single-line text.
static void text(const char* string, uint32_t color, float x, float y)
{
    g_text_buffer.start(color, x, y);

    utf8_int32_t codepoint = 0;

    for (void* it = utf8codepoint(string, &codepoint); codepoint; it = utf8codepoint(it, &codepoint))
    {
        // TODO : The codepoint-to-index should be handled by the glyph cache.
        if (BX_LIKELY(codepoint >= 32 && codepoint <= 126))
        {
            g_text_buffer.add(codepoint - 32);
        }
    }

    g_text_buffer.end();
}

// Single-line text.
static void text_size(const char* string, float& out_width, float& out_height)
{
    g_cache.get_size(out_width, out_height);

    out_width *= (float)utf8size_lazy(string);
}

static bool tab(uint8_t id, const Rect& rect, const char* label)
{
    State      state   = STATE_COLD;
    const bool clicked = button_logic(id, rect, true, state);

    constexpr uint32_t colors[] =
    {
        0xff0000ff,
        0x00ff00ff,
        0x0000ffff,
    };

    ::rect(colors[state], rect);

    float width;
    float height;
    text_size(label, width, height);

    text(label, 0xffffffff, (rect.x0 + rect.x1 - width) * 0.5f, (rect.y0 + rect.y1 - height) * 0.5f);

    return clicked;
}

static void update_gui()
{
    ASSERT(g_current_stack.empty());

    if (!(mouse_down(MOUSE_LEFT) || mouse_held(MOUSE_LEFT)))
    {
        g_active_stack.clear();
    }

    if (BX_UNLIKELY(g_color_rect_list.empty()))
    {
        return;
    }

    identity();

    begin_mesh(GUI_RECT_MESH, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR);
    {
        for (const ColorRect& rect : g_color_rect_list)
        {
            color(rect.color);

            vertex(rect.rect.x0, rect.rect.y0, 0.0f);
            vertex(rect.rect.x0, rect.rect.y1, 0.0f);
            vertex(rect.rect.x1, rect.rect.y1, 0.0f);
            vertex(rect.rect.x1, rect.rect.y0, 0.0f);
        }
    }
    end_mesh();

    g_color_rect_list.clear();

    state(STATE_WRITE_RGB);
    mesh(GUI_RECT_MESH);
}


// -----------------------------------------------------------------------------
// MINIMO CALLBACKS
// -----------------------------------------------------------------------------

static void setup()
{
    // vsync(1);

    title("MiNiMo Editor");

    pass(DEFAULT_PASS);

    clear_color(0x303030ff);
    clear_depth(1.0f);

    create_font(GUI_FONT, g_font_data);

    // TODO : Add `MiNiMo` support for backend-specific shader selection.
#if BX_PLATFORM_OSX
    create_shader(GUI_TEXT_SHADER, text_vs_mtl , sizeof(text_vs_mtl ), text_fs_mtl , sizeof(text_fs_mtl ));
#elif BX_PLATFORM_WINDOWS
    create_shader(GUI_TEXT_SHADER, text_vs_dx11, sizeof(text_vs_dx11), text_fs_dx11, sizeof(text_fs_dx11));
#endif

    create_uniform(GUI_TEXT_INFO_UNIFORM, UNIFORM_VEC4, "u_atlas_info");
}

static void update()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    if (dpi_changed())
    {
        g_cache.rebuild(8.0f);
    }

    pass(DEFAULT_PASS);

    identity();
    ortho(0.0f, width(), height(), 0.0f, 1.0f, -1.0f);
    projection();

    if (tab(ID, { 100.0f, 50.0f, 250.0f, 75.0f }, "First"))
    {
        printf("First!\n");
    }

    if (tab(ID, { 275.0f, 50.0f, 425.0f, 75.0f }, "Second"))
    {
        printf("Second!\n");
    }

    update_gui();

    g_text_buffer.submit();
}


// -----------------------------------------------------------------------------
// MINIMO MAIN ENTRY
// -----------------------------------------------------------------------------

MNM_MAIN(nullptr, setup, update, nullptr);
