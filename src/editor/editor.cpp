#include <assert.h>      // assert
#include <stdint.h>      // uint*_t

#include <vector>        // vector

#include <mnm/mnm.h>

#include "editor_font.h" // g_editor_*
#include "editor_ui.h"   // Editor


// -----------------------------------------------------------------------------
// RESOURCE IDS
// -----------------------------------------------------------------------------

#define FONT_ID  127

#define ATLAS_ID 1023

#define TEXT_ID  4095

#define LINES_ID 4094


// -----------------------------------------------------------------------------
// UTILITY MACROS
// -----------------------------------------------------------------------------

// TODO : Better assert.
#define ASSERT(cond) assert(cond)


// -----------------------------------------------------------------------------
// TYPE ALIASES
// -----------------------------------------------------------------------------

template <typename T>
using Vector = std::vector<T>;


// -----------------------------------------------------------------------------
// GUI HELPERS
// -----------------------------------------------------------------------------

enum struct State : uint8_t
{
    COLD,
    HOT,
    ACTIVE,
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

    inline uint8_t pop()
    {
        ASSERT(m_size > 0);
        return m_stack[--m_size];
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

static bool button_logic(uint8_t id, const Rect& rect, bool enabled, State& out_state)
{
    out_state = State::COLD;

    if (mouse_over(rect) && g_active_stack.empty())
    {
        out_state = State::HOT;

        if (mouse_down(MOUSE_LEFT))
        {
            g_active_stack.push(id);
            out_state = State::ACTIVE;
        }
    }

    return
        mouse_up(MOUSE_LEFT) && g_active_stack.top() == id && mouse_over(rect);
}


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
    begin_atlas(ATLAS_ID, ATLAS_H_OVERSAMPLE_2X | ATLAS_NOT_THREAD_SAFE, FONT_ID, g_tes.font_cap_height * dpi());
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
