#include <assert.h>      // assert
#include <stdint.h>      // uint*_t

#include <vector>        // vector

#include <mnm/mnm.h>

#include "editor_font.h" // g_editor_*
#include "editor_ui.h"   // Editor


// -----------------------------------------------------------------------------
// RESOURCE IDS
// -----------------------------------------------------------------------------

#define FONT_ID      127

#define FBUF_ID      127

#define ATLAS_ID     1023

#define CACHE_ID     1022

#define TEXT_ID      4095

#define RECTS_ID     4094

#define PASS_CACHE   62

#define PASS_DEFAULT 63


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
// GUI HELPERS
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

struct ColorRect
{
    uint32_t color = 0x00000000;
    Rect     rect;
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

static bool tab(uint8_t id, const Rect& rect, const char* text)
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

    // TODO : Issue text.

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

    begin_mesh(RECTS_ID, MESH_TRANSIENT | PRIMITIVE_QUADS | VERTEX_COLOR);
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
    mesh(RECTS_ID);
}


// -----------------------------------------------------------------------------
// GLYPH CACHE
//
// We take advantage of the monospaced font and let `MiNiMo` render the glyphs
// into a texture where each glyph rectangle is of the same size. This is a bit
// wasteful on texture space size, but will enable us to use much simpler
// rendering logic and also simplifies all the text size and placement related
// calculations.
// -----------------------------------------------------------------------------

struct GlyphPosition
{
    uint8_t x = 0;
    uint8_t y = 0;
};

struct GlyphCache
{
    int           texture_size = 0;
    float         glyph_width  = 0.0f;
    float         glyph_height = 0.0f;
    GlyphPosition ascii[95];

    void rebuild(float cap_height)
    {
        // TODO : `ATLAS_ALLOW_UPDATE` seems to be broken again.
        begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE, FONT_ID, cap_height * dpi());
        glyph_range(0x20, 0x7e);
        end_atlas();

        text_size(ATLAS_ID, "X", 0, 1.0f, &glyph_width, &glyph_height);

        for (texture_size = 128; ; texture_size *= 2)
        {
            // TODO : Rounding and padding.
            const int cols = (int)(texture_size / glyph_width );
            const int rows = (int)(texture_size / glyph_height);

            if (cols * rows >= BX_COUNTOF(ascii))
            {
                break;
            }
        }

        begin_text(TEXT_ID, ATLAS_ID, TEXT_TRANSIENT | TEXT_V_ALIGN_CAP_HEIGHT);
        {
            color(0xffffffff);

            const uint8_t cols = (uint8_t)(texture_size / glyph_width);

            for (char i = 0, j = 32; i < BX_COUNTOF(ascii); i++, j++)
            {
                const uint8_t x = i % cols;
                const uint8_t y = i / cols;

                ascii[i] = { x, y };

                identity();
                translate(x * glyph_width, y * glyph_height, 0.0f);

                char letter[2] = { j, 0 };
                text(letter, 0);
            }
        }
        end_text();

        create_texture(CACHE_ID, TEXTURE_R8 | TEXTURE_CLAMP | TEXTURE_TARGET, texture_size, texture_size);

        begin_framebuffer(FBUF_ID);
        texture(CACHE_ID);
        end_framebuffer();

        pass(PASS_CACHE);

        framebuffer(FBUF_ID);
        clear_color(0x000000ff); // TODO : If we dynamically update the cache, we only have to clear before the first draw.
        viewport(0, 0, texture_size, texture_size);

        identity();
        ortho(0.0f, texture_size, texture_size, 0.0f, 1.0f, -1.0f);
        projection();

        identity();
        mesh(TEXT_ID);
    }
};

static GlyphCache g_cache;


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
    // vsync(1);

    title("MiNiMo Editor");

    pass(PASS_DEFAULT);

    clear_color(0x303030ff);
    clear_depth(1.0f);

    create_font(FONT_ID, g_font_data);
}

static void update()
{
    if (key_down(KEY_ESCAPE))
    {
        quit();
    }

    if (frame() == 0) // dpi_changed())
    {
        g_cache.rebuild(8.0f);
    }

    pass(PASS_DEFAULT);

    identity();
    ortho(0.0f, width(), height(), 0.0f, 1.0f, -1.0f);
    projection();

    if (tab(ID, { 100.0f, 50.0f, 250.0f, 75.0f }, "Untitled"))
    {
        printf("ACTION!\n");
    }

    if (tab(ID, { 275.0f, 50.0f, 425.0f, 75.0f }, "Untitled"))
    {
        printf("ACTION!\n");
    }

    update_gui();
}


// -----------------------------------------------------------------------------
// MINIMO MAIN ENTRY
// -----------------------------------------------------------------------------

MNM_MAIN(nullptr, setup, update, nullptr);
