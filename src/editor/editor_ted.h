#pragma once

struct TextEditor
{
    enum struct DisplayMode
    {
        RIGHT,
        LEFT,
        OVERLAY,
    };

    ted::State     state;
    ted::Clipboard clipboard;
    double         blink_base_time = 0.0;
    float          split_x         = 0.0f; // Screen coordinates.
    float          scroll_offset   = 0.0f; // Lines (!).
    float          handle_position = 0.0f;
    DisplayMode    display_mode    = DisplayMode::RIGHT;

    void set_content(const char* string)
    {
        state.clear();
        state.paste(string);
        state.cursors[0] = {};
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

        // Input handling ------------------------------------------------------
        // TODO : Process keys, mouse clicks/drags, and clipboard handling.
        
        if      (key_down(KEY_LEFT )) { state.action(ted::Action::MOVE_LEFT ); }
        else if (key_down(KEY_RIGHT)) { state.action(ted::Action::MOVE_RIGHT); }
        else if (key_down(KEY_UP   )) { state.action(ted::Action::MOVE_UP   ); }
        else if (key_down(KEY_DOWN )) { state.action(ted::Action::MOVE_DOWN ); }

        // Line number format --------------------------------------------------
        char  line_number[8];
        char  line_format[8];
        float line_number_width = 0.0f;

        for (size_t i = state.lines.size(), j = 0; ; i /= 10, j++)
        {
            if (i == 0)
            {
                bx::snprintf(line_format, sizeof(line_format), "%%%izu ", bx::max<size_t>(j + 1, 3));
                bx::snprintf(line_number, sizeof(line_number), line_format, 1);

                line_number_width = state.char_width * bx::strLen(line_number);

                break;
            }
        }

        // Screen divider ------------------------------------------------------
        if (split_x == 0.0f)
        {
            split_x = width * 0.5f;
        }

        if (display_mode != DisplayMode::OVERLAY)
        {
            ctx.vdivider(ID, split_x, 0.0f, height, divider_thickness);
        }

        split_x = bx::round(split_x * dpi) / dpi;

        // Viewport ------------------------------------------------------------
        Rect viewport;
        switch (display_mode)
        {
        case DisplayMode::RIGHT:
            viewport = { split_x + divider_thickness, 0.0f, width, height };
            break;
        case DisplayMode::LEFT:
            viewport = { 0.0f, 0.0f, split_x, height};
            break;
        case DisplayMode::OVERLAY:
            viewport = { 0.0f, 0.0f, width, height };
            break;
        }

        ctx.rect(COLOR_BLACK, viewport);

        // Scrollbar -----------------------------------------------------------
        const float max_scroll  = bx::max(0.0f, state.lines.size() - 1.0f);

        if (state.lines.size() > 0)
        {
            const float handle_size = bx::max(viewport.height() * viewport.height() /
                (max_scroll  * state.line_height + viewport.height()), min_handle_size);

            ctx.scrollbar(
                ID,
                { viewport.x1 - scrollbar_width, viewport.y0, viewport.x1, viewport.y1 },
                handle_position,
                handle_size,
                scroll_offset,
                0.0f,
                max_scroll
            );
        }

        if (viewport.is_hovered() && ctx.none_active())
        {
            if (scroll_y())
            {
                scroll_offset = bx::clamp(scroll_offset - scroll_y() * scrolling_speed / state.line_height, 0.0f, max_scroll);
            }

            // TODO : Exclude scrollbar and line number areas.
            if ((mouse_x() >= viewport.x0 + line_number_width) &&
                (mouse_x() <  viewport.x1 - scrollbar_width))
            {
                ctx.cursor = CURSOR_I_BEAM;
            }
        }

        // Text and line numbers -----------------------------------------------
        const size_t first_line = static_cast<size_t>(bx::floor(scroll_offset));
        const size_t line_count = static_cast<size_t>(bx::ceil(viewport.height() / state.line_height)) + 1;
        const size_t last_line  = bx::min(first_line + line_count, state.lines.size());

        const uint32_t max_chars = static_cast<uint32_t>(bx::max(1.0f,
            bx::ceil((viewport.width() - line_number_width - scrollbar_width) / state.char_width)));

        float y = round_to_pixel(viewport.y0 - bx::fract(scroll_offset) * state.line_height, dpi);

        for (size_t i = first_line; i < last_line; i++, y += state.line_height)
        {
            bx::snprintf(line_number, sizeof(line_number), line_format, i);

            ctx.text(line_number, COLOR_EDITOR_LINE_NUMBER, viewport.x0, y);

            ctx.text(
                state.buffer.data() + state.lines[i].start,
                state.buffer.data() + state.lines[i].end,
                max_chars,
                COLOR_EDITOR_TEXT,
                viewport.x0 + line_number_width,
                y
            );
        }

        // Selections and carets -----------------------------------------------
        const ted::Range visible_range = { state.lines[first_line].start, state.lines[last_line].start };

        if (bx::fract(static_cast<float>(elapsed() - blink_base_time)) < 0.5f)
        {
            for (size_t i = 0; i < state.cursors.size(); i++)
            {
                using namespace ted;

                if (range_contains(visible_range, state.cursors[i].offset))
                {
                    const Position position = to_position(state, state.cursors[i].offset, first_line);

                    const float x = viewport.x0 + line_number_width + state.char_width * position.x;
                    const float y = viewport.y0 - bx::mod(scroll_offset, state.line_height) + state.line_height * (position.y - first_line);

                    ctx.rect(COLOR_RED, { x - caret_width * 0.5f, y, x + caret_width * 0.5f, y + state.line_height });
                }
            }
        }

        // ...

        ctx.pop_id();
    }
};
