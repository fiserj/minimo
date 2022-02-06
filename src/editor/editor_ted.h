#pragma once

#include <inttypes.h> // PRIu32

struct Command
{
    enum Enum
    {
        // The first block has to be copy of `mnm::tes::Action`.
        MOVE_LEFT,
        MOVE_RIGHT,
        MOVE_UP,
        MOVE_DOWN,
        SELECT_LEFT,
        SELECT_RIGHT,
        SELECT_UP,
        SELECT_DOWN,
        DELETE_LEFT,
        DELETE_RIGHT,
        GO_BACK,
        GO_FORWARD,
        MOVE_LINE_UP,
        MOVE_LINE_DOWN,
        CANCEL_SELECTION,
        SELECT_ALL,
        SELECT_WORD,
        SELECT_LINE,
        NEW_LINE,
        TAB,
        CLEAR_HISTORY,
        UNDO,
        REDO,

        CLICK,
        CLICK_MULTI,
        CLICK_WITH_SHIFT,
        DRAG,
        COPY,
        CUT,
        PASTE,

        COUNT, // Don't use.
    };
};

// struct CommandBuffer
// {
//     std::vector<uint8_t> buffer;
//     uint32_t               head;

//     CommandBuffer()
//     {
//         clear();
//     }

//     void clear()
//     {
//         buffer.clear();
//         buffer.reserve(4096);

//         rewind();
//     }

//     void rewind()
//     {
//         head = 0;
//     }

//     void align(uint32_t alignment)
//     {
//         assert((alignment & (alignment - 1)) == 0);

//         const uint32_t mask = alignment - 1;
//         const uint32_t size = (buffer.size + mask) & (~mask);

//         buffer.resize(size);
//     }

//     void write(const void* data, uint32_t size)
//     {
//         const uint32_t offset = buffer.size;

//         buffer.resize(offset + size);

//         bx::memCopy(&buffer[offset], data, size);
//     }

//     template <typename T>
//     void write(const T& value)
//     {
//         align(BX_ALIGNOF(T));
//         write(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
//     }

//     void read(void* data, uint32_t size)
//     {
//         assert(head + size <= buffer.size);

//         bx::memCopy(data, &buffer[head], size);

//         head += size;
//     }

//     template <typename T>
//     void read(T& value)
//     {
//         align(BX_ALIGNOF(T));
//         read(reinterpret_cast<uint8_t*>(&value), sizeof(T));
//     }
// };

struct Modifier
{
    enum Enum : char
    {
        ALT     = 0x01,
        CONTROL = 0x02,
        SHIFT   = 0x04,
        SUPER   = 0x08,
    };
};

enum : char
{
    LMB_DOWN     = -1,
    LMB_HELD     = -2,
    LMB_CLICK_2X = -3,
    LMB_CLICK_3X = -4,
};

struct KeyBinding
{
    char key  = 0;
    char mods = 0;
};

static const int s_mod_to_key[9][2] =
{
    {},
    { KEY_ALT_LEFT    , KEY_ALT_RIGHT     },
    { KEY_CONTROL_LEFT, KEY_CONTROL_RIGHT },
    {},
    { KEY_SHIFT_LEFT  , KEY_SHIFT_RIGHT   },
    {},
    {},
    {},
    { KEY_SUPER_LEFT  , KEY_SUPER_RIGHT   },
};

static inline bool is_mod_active(Modifier::Enum mod)
{
    return
        key_held(s_mod_to_key[mod][0]) ||
        key_held(s_mod_to_key[mod][1]) ||
        key_down(s_mod_to_key[mod][0]) ||
        key_down(s_mod_to_key[mod][1]) ;
}

// TODO : The inputs should probably be cached to save the function calls.
static bool is_active(KeyBinding binding)
{
    bool active;

    switch (binding.key)
    {
    case LMB_DOWN:
        active = mouse_down(MOUSE_LEFT);
        break;

    case LMB_HELD:
        active = mouse_held(MOUSE_LEFT) && (mouse_dx() || mouse_dy());
        break;

    case LMB_CLICK_2X:
        active = (2 == mouse_clicked(MOUSE_LEFT));
        break;

    case LMB_CLICK_3X:
        active = (3 == mouse_clicked(MOUSE_LEFT));
        break;

    default:
        active = key_down(binding.key) || key_repeated(binding.key);
    }

    if (active && binding.mods > 0)
    {
        for (int i = 0, j = 1; i < 4 && active; i++, j = 1 << i)
        {
            // TODO : We should also check that no unspecified modifier is active.
            active = !(binding.mods & j) || is_mod_active(static_cast<Modifier::Enum>(j));
        }
    }

    return active;
}

struct TextEditor
{
    enum struct DisplayMode
    {
        RIGHT,
        LEFT,
        OVERLAY,
    };

    TSParser*           parser           = nullptr;
    TSTree*             tree             = nullptr; // TODO : This has to be updated and cleared!
    TSTreeCursor        tree_cursor      = {};
    mnm::tes::State     state;
    mnm::tes::Clipboard clipboard;
    KeyBinding          bindings[Command::COUNT];
    Command::Enum       commands[Command::COUNT];
    double              blink_base_time  = 0.0;
    float               split_x          = 0.0f; // Screen coordinates.
    float               scroll_offset    = 0.0f; // Lines (!).
    float               handle_position  = 0.0f;
    DisplayMode         display_mode     = DisplayMode::RIGHT;
    bool                viewport_clicked = false;

    TextEditor(const TextEditor&) = delete;

    TextEditor& operator=(const TextEditor&) = delete;

    ~TextEditor()
    {
        ts_tree_cursor_delete(&tree_cursor);
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    TextEditor()
    {
        parser = ts_parser_new();
        ts_parser_set_language(parser, tree_sitter_c());

#if BX_PLATFORM_OSX
        constexpr char PLATFORM_MOD = Modifier::SUPER;
#else
        constexpr char PLATFORM_MOD = Modifier::CONTROL;
#endif

        bindings[Command::MOVE_LEFT       ] = { KEY_LEFT                       };
        bindings[Command::MOVE_RIGHT      ] = { KEY_RIGHT                      };
        bindings[Command::MOVE_UP         ] = { KEY_UP                         };
        bindings[Command::MOVE_DOWN       ] = { KEY_DOWN                       };
        bindings[Command::SELECT_LEFT     ] = { KEY_LEFT     , Modifier::SHIFT };
        bindings[Command::SELECT_RIGHT    ] = { KEY_RIGHT    , Modifier::SHIFT };
        bindings[Command::SELECT_UP       ] = { KEY_UP       , Modifier::SHIFT };
        bindings[Command::SELECT_DOWN     ] = { KEY_DOWN     , Modifier::SHIFT };
        bindings[Command::DELETE_LEFT     ] = { KEY_BACKSPACE                  };
        bindings[Command::DELETE_RIGHT    ] = { KEY_DELETE                     };
        bindings[Command::GO_BACK         ] = { KEY_LEFT     , Modifier::ALT   };
        bindings[Command::GO_FORWARD      ] = { KEY_RIGHT    , Modifier::ALT   };
        bindings[Command::MOVE_LINE_UP    ] = { KEY_UP       , Modifier::ALT   };
        bindings[Command::MOVE_LINE_DOWN  ] = { KEY_DOWN     , Modifier::ALT   };
        bindings[Command::CANCEL_SELECTION] = { KEY_ESCAPE                     };
        bindings[Command::SELECT_ALL      ] = { 'A'          , PLATFORM_MOD    };
        bindings[Command::SELECT_WORD     ] = { LMB_CLICK_2X                   };
        bindings[Command::SELECT_LINE     ] = { LMB_CLICK_3X                   };
        bindings[Command::NEW_LINE        ] = { KEY_ENTER                      };
        bindings[Command::TAB             ] = { KEY_TAB                        };
        bindings[Command::CLICK           ] = { LMB_DOWN                       };
        bindings[Command::CLICK_MULTI     ] = { LMB_DOWN     , Modifier::ALT   };
        bindings[Command::CLICK_WITH_SHIFT] = { LMB_DOWN     , Modifier::SHIFT };
        bindings[Command::DRAG            ] = { LMB_HELD                       };
        bindings[Command::COPY            ] = { 'C'          , PLATFORM_MOD    };
        bindings[Command::CUT             ] = { 'X'          , PLATFORM_MOD    };
        bindings[Command::PASTE           ] = { 'V'          , PLATFORM_MOD    };
        bindings[Command::UNDO            ] = { 'Z'          , PLATFORM_MOD    };

        commands[ 0] = Command::DRAG;
        commands[ 1] = Command::CLICK_WITH_SHIFT;
        commands[ 2] = Command::SELECT_WORD;
        commands[ 3] = Command::SELECT_LINE;
        commands[ 4] = Command::CLICK_MULTI;
        commands[ 5] = Command::CLICK;
        commands[ 6] = Command::COPY;
        commands[ 7] = Command::CUT;
        commands[ 8] = Command::PASTE;
        commands[ 9] = Command::DELETE_LEFT;
        commands[10] = Command::DELETE_RIGHT;
        commands[11] = Command::NEW_LINE;
        commands[12] = Command::TAB;
        commands[13] = Command::CANCEL_SELECTION;
        commands[14] = Command::SELECT_ALL;
        commands[15] = Command::SELECT_LEFT;
        commands[16] = Command::SELECT_RIGHT;
        commands[17] = Command::SELECT_UP;
        commands[18] = Command::SELECT_DOWN;
        commands[19] = Command::GO_BACK;
        commands[20] = Command::GO_FORWARD;
        commands[21] = Command::MOVE_LINE_UP;
        commands[22] = Command::MOVE_LINE_DOWN;
        commands[23] = Command::MOVE_LEFT;
        commands[24] = Command::MOVE_RIGHT;
        commands[25] = Command::MOVE_UP;
        commands[26] = Command::MOVE_DOWN;
        commands[27] = Command::UNDO;
    }

    void set_content(const char* string)
    {
        state.clear();
        state.paste(string);
        state.cursors[0] = {};
        state.action(mnm::tes::Action::CLEAR_HISTORY);

        tree = ts_parser_parse_string(
            parser,
            tree,
            state.buffer.data,
            state.buffer.size - 1
        );
    }

    void update(gui::Context& ctx, uint8_t id)
    {
        using namespace gui;

        constexpr float caret_width       =  2.0f;
        constexpr float divider_thickness =  4.0f;
        constexpr float scrollbar_width   = 10.0f;
        constexpr float scrolling_speed   = 10.0f; // TODO : Is this cross-platform stable ?
        constexpr float min_handle_size   = 20.0f;

        const float width  = ::width();
        const float height = ::height();
        const float dpi    = ::dpi();

        ctx.push_id(id);

        // Properties' update --------------------------------------------------
        state.char_width  = ctx.glyph_cache.glyph_screen_width ();
        state.line_height = ctx.glyph_cache.glyph_screen_height();

        // Screen divider ------------------------------------------------------
        bool gui_active = false;

        if (split_x == 0.0f)
        {
            split_x = width * 0.5f;
        }

        if (display_mode != DisplayMode::OVERLAY)
        {
            gui_active = ctx.vdivider(ID, split_x, 0.0f, height, divider_thickness);
        }

        split_x = round_to_pixel(split_x, dpi);

        // Viewport ------------------------------------------------------------
        Rect viewport;
        switch (display_mode)
        {
        case DisplayMode::RIGHT:
            viewport = { split_x + divider_thickness, 0.0f, width, height };
            break;
        case DisplayMode::LEFT:
            viewport = { 0.0f, 0.0f, split_x, height };
            break;
        case DisplayMode::OVERLAY:
            viewport = { 0.0f, 0.0f, width, height };
            break;
        }

        const float bar_height = round_to_pixel(8.0f + state.line_height, dpi);

        viewport.y1 -= bar_height;

        ctx.rect(COLOR_BLACK, viewport);

        // Line number format --------------------------------------------------
        char  line_number[8];
        char  line_format[8];
        float line_number_width = 0.0f;

        for (uint32_t i = state.lines.size, j = 0; ; i /= 10, j++)
        {
            if (i == 0)
            {
                bx::snprintf(line_format, sizeof(line_format), "%%%" PRIu32 PRIu32 " ", bx::max(j + 1, UINT32_C(3)));
                bx::snprintf(line_number, sizeof(line_number), line_format, 1);

                line_number_width = state.char_width * bx::strLen(line_number);

                break;
            }
        }

        // Scrollbar -----------------------------------------------------------
        const float max_scroll = bx::max(0.0f, state.lines.size - 1.0f);

        if (state.lines.size > 0)
        {
            const float handle_size = bx::max(viewport.height() * viewport.height() /
                (max_scroll  * state.line_height + viewport.height()), min_handle_size);

            gui_active = ctx.scrollbar(
                ID,
                { viewport.x1 - scrollbar_width, viewport.y0, viewport.x1, viewport.y1 },
                handle_position,
                handle_size,
                scroll_offset,
                0.0f,
                max_scroll
            ) || gui_active;
        }

        if (viewport.is_hovered() && ctx.none_active())
        {
            if (scroll_y())
            {
                scroll_offset = bx::clamp(scroll_offset - scroll_y() * scrolling_speed / state.line_height, 0.0f, max_scroll);
            }

            if ((mouse_x() >= viewport.x0 + line_number_width) &&
                (mouse_x() <  viewport.x1 - scrollbar_width))
            {
                ctx.cursor = CURSOR_I_BEAM;
            }
        }

        // Input handling ------------------------------------------------------
        // TODO : Only process events if viewport is active / focused.
        if (mouse_down(MOUSE_LEFT))
        {
            viewport_clicked = viewport.is_hovered();
        }

        if (!gui_active)
        {
            const float x = mouse_x() - viewport.x0 - line_number_width;
            const float y = mouse_y() - viewport.y0 + state.line_height * scroll_offset;

            bool jump_to_cursor = false;

            while (uint32_t value = codepoint())
            {
                state.codepoint(value);

                jump_to_cursor = true;
            }

            for (int i = 0; i < Command::COUNT; i++)
            {
                if (is_active(bindings[commands[i]]))
                {
                    switch (commands[i])
                    {
                    case Command::COPY:
                        state.copy(clipboard);
                        break;

                    case Command::CUT:
                        state.cut(clipboard);
                        break;

                    case Command::PASTE:
                        state.paste(clipboard);
                        break;

                    case Command::CLICK:
                        if (viewport_clicked)
                        {
                            state.click(x, y, false);
                        }
                        break;

                    case Command::CLICK_MULTI:
                        if (viewport_clicked)
                        {
                            state.click(x, y, true);
                        }
                        break;

                    case Command::CLICK_WITH_SHIFT:
                    case Command::DRAG:
                        if (viewport_clicked)
                        {
                            state.drag(x, y);
                        }
                        break;

                    default:
                        state.action(mnm::tes::Action(commands[i]));
                    }

                    jump_to_cursor = true;

                    break;
                }
            }

            if (jump_to_cursor)
            {
                const uint32_t cursor = key_down(KEY_DOWN) ? state.cursors.size - 1 : 0;
                const float  line   = static_cast<float>(mnm::tes::to_position(
                    state,
                    state.cursors[cursor].offset
                ).y);

                scroll_offset = bx::min(line, bx::max(0.0f, scroll_offset,
                    (line + 2) - viewport.height() / state.line_height));

                blink_base_time = elapsed();
            }
        }

        // Selections ----------------------------------------------------------
        const uint32_t first_line = static_cast<uint32_t>(bx::floor(scroll_offset));
        const uint32_t line_count = static_cast<uint32_t>(bx::ceil(viewport.height() / state.line_height)) + 1;
        const uint32_t last_line  = bx::min(first_line + line_count, state.lines.size - 1);

        const mnm::tes::Range visible_range = { state.lines[first_line].start, state.lines[last_line].end };

        for (uint32_t i = 0, search_line = first_line; i < state.cursors.size; i++)
        {
            using namespace mnm::tes;

            const Range visible_selection = range_intersection(state.cursors[i].selection, visible_range);

            if (!range_empty(visible_selection))
            {
                Position position = to_position(state, visible_selection.start, search_line);

                float  y     = round_to_pixel(viewport.y0 + (position.y - first_line - bx::fract(scroll_offset)) * state.line_height, dpi);
                float  x0    = viewport.x0 + line_number_width + position.x * state.char_width;
                uint32_t start = visible_selection.start;

                for (;;)
                {
                    const uint32_t end    = bx::min(visible_selection.end, state.lines[position.y].end);
                    const uint32_t length = mnm::utf8_length(line_string(state, position.y), end - start);

                    const float x1 = x0 + state.char_width * length;

                    ctx.rect(COLOR_GREEN, { x0, y, x1, y + state.line_height });

                    if (end == visible_selection.end)
                    {
                        break;
                    }

                    y    += state.line_height;
                    x0    = viewport.x0 + line_number_width;
                    start = end;

                    position.x = 0;
                    position.y ++;
                }

                search_line = position.y;
            }
        }

        // Text and line numbers -----------------------------------------------
        ctx.push_clip({ viewport.x0, viewport.y0, viewport.x1 - scrollbar_width, viewport.y1 });

        const uint32_t max_chars = static_cast<uint32_t>(bx::max(1.0f,
            bx::ceil((viewport.width() - line_number_width - scrollbar_width) / state.char_width)));

        float y = round_to_pixel(viewport.y0 - bx::fract(scroll_offset) * state.line_height, dpi);

        if (tree_cursor.tree)
        {
            ts_tree_cursor_reset(&tree_cursor, ts_tree_root_node(tree));
        }
        else
        {
            tree_cursor = ts_tree_cursor_new(ts_tree_root_node(tree));
        }

        mnm::lay_syntax_highlighted_text(
            ctx,
            viewport.x0 + line_number_width,
            y,
            state,
            tree_cursor,
            first_line,
            last_line,
            max_chars
        );

        for (uint32_t i = first_line; i <= last_line; i++, y += state.line_height)
        {
            bx::snprintf(line_number, sizeof(line_number), line_format, i);

            ctx.text(line_number, COLOR_EDITOR_LINE_NUMBER, viewport.x0, y);
        }

        ctx.pop_clip();

        // Carets --------------------------------------------------------------
        if (bx::fract(static_cast<float>(elapsed() - blink_base_time)) < 0.5f)
        {
            for (uint32_t i = 0; i < state.cursors.size; i++)
            {
                using namespace mnm::tes;

                if (range_contains(visible_range, state.cursors[i].offset))
                {
                    const Position position = to_position(state, state.cursors[i].offset, first_line);

                    const float x = viewport.x0 + line_number_width + state.char_width * position.x;
                    const float y = round_to_pixel(viewport.y0 + (position.y - first_line - bx::fract(scroll_offset)) * state.line_height, dpi);

                    ctx.rect(COLOR_RED, { x - caret_width * 0.5f, y, x + caret_width * 0.5f, y + state.line_height });
                }
            }
        }

        // Status bar ----------------------------------------------------------
        {
            const gui::Rect bar_rect = { viewport.x0, viewport.y1, viewport.x1, height };

            ctx.push_clip(bar_rect);
            ctx.rect(COLOR_RED, bar_rect);

            const float x = round_to_pixel(bar_rect.x0 + state.char_width, dpi);
            const float y = round_to_pixel(bar_rect.y0 + (bar_height - state.line_height) * 0.5f, dpi);

            ctx.text("Status Bar ...", COLOR_EDITOR_TEXT, x, y);
            ctx.pop_clip();
        }

        // ...

        ctx.pop_id();
    }
};
