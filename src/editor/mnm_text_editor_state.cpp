#ifndef UINT32_MAX
#   include <stdint.h>
#endif

// TODO : Wrap with own `MNM_STD_TYPE_TRAITS_INCLUDED` macro?
#include <type_traits>

#ifndef BX_ALLOCATOR_H_HEADER_GUARD
#   include <bx/allocator.h>
#endif

#ifndef BX_SORT_H_HEADER_GUARD
#   include <bx/sort.h>
#endif

#ifndef MNM_ARRAY_INCLUDED
#   include <mnm_array.h>
#endif

#ifndef MNM_UTF8_INCLUDED
#   include <mnm_utf8.h>
#endif

#ifndef MNM_TEXT_EDITOR_STATE_INCLUDED
#   include "mnm_text_editor_state.h"
#endif

namespace mnm
{

namespace tes
{

// -----------------------------------------------------------------------------
// INTERNAL HELPERS
// -----------------------------------------------------------------------------

struct Position
{
    uint32_t x;
    uint32_t y;
};

static inline void range_fix(Range& range)
{
    if (range.end < range.start)
    {
        bx::swap(range.start, range.end);
    }
}

static inline bool range_empty(const Range& range)
{
    return range.start == range.end;
}

static inline uint32_t range_size(const Range& range)
{
    return range.end - range.start;
}

static inline bool range_contains(const Range& range, uint32_t offset)
{
    return range.start <= offset && range.end + range_empty(range) > offset;
}

static bool range_overlap(const Range& first, const Range& second)
{
    return
        (first.start >= second.start && first.end   <= second.end) ||
        (first.start >= second.start && first.start <= second.end) ||
        (first.end   >= second.start && first.end   <= second.end) ;
}

static Range range_intersection(const Range& first, const Range& second)
{
    Range range;
    range.start = bx::max(first.start, second.start);
    range.end   = bx::min(first.end  , second.end  );

    if (range.start > range.end)
    {
        range.start = 0.0f;
        range.end   = 0.0f;
    }

    return range;
}

static inline const char* line_string(const State& state, uint32_t line)
{
    return state.buffer.data + state.lines[line].start;
}

static inline uint32_t line_length(const State& state, uint32_t line)
{
    return utf8_length(line_string(state, line), range_size(state.lines[line]));
}

static uint32_t to_line(const Array<Range>& lines, uint32_t offset, uint32_t start_line = 0)
{
    for (uint32_t i = start_line; i < lines.size; i++)
    {
        if (range_contains(lines[i], offset))
        {
            return i;
        }
    }

    return 0;
}

static uint32_t to_offset(const State& state, uint32_t x, uint32_t y)
{
    const char* string = line_string(state, y);
    const char* start  = string;
    uint32_t    codepoint;

    while ((codepoint = utf8_next_codepoint(string)) && codepoint != '\n' && x--)
    {
    }

    return uint32_t(string - start);
}

static uint32_t to_line(const State& state, uint32_t offset, uint32_t start_line = 0)
{
    for (uint32_t i = start_line; i < state.lines.size; i++)
    {
        if (range_contains(state.lines[i], offset))
        {
            return i;
        }
    }

    ASSERT(false && "Cannot find line containing given offset.");

    return 0;
}

static Position to_position(const State& state, uint32_t offset, uint32_t start_line = 0)
{
    Position position = {};

    for (uint32_t i = start_line; i < state.lines.size; i++)
    {
        if (range_contains(state.lines[i], offset))
        {
            position.y = i;
            position.x = utf8_length(line_string(state, i), offset - state.lines[i].start);

            break;
        }
    }

    return position;
}

static inline uint32_t to_column(const State& state, uint32_t offset, uint32_t line)
{
    ASSERT(range_contains(state.lines[line], offset));

    return utf8_length(line_string(state, line), offset - state.lines[line].start);
}

static Position click_position(const State& state, float x, float y)
{
    x = bx::max(x, 0.0f) / state.char_width;
    y = bx::max(y, 0.0f) / state.line_height;

    const uint32_t yi = bx::min(uint32_t(y), state.lines.size - 1);
    const uint32_t xi = bx::min(uint32_t(x + 0.5f), line_length(state, yi) - 1);

    return { xi, yi };
}

static uint32_t resize_selection(Array<char>& buffer, const Range& selection, uint32_t new_size)
{
    const uint32_t old_size = range_size(selection);

    if (new_size != old_size)
    {
        const uint32_t src  = selection.end;
        const uint32_t dst  = selection.start + new_size;
        const uint32_t span = buffer.size - src;

        if (new_size > old_size)
        {
            buffer.resize(buffer.size + new_size - old_size);
        }

        bx::memMove(buffer.data + dst, buffer.data + src, span);
    }

    return new_size - old_size;
}

static uint32_t paste_string(State& state, Cursor& cursor, const char* string, uint32_t size, uint32_t times = 1)
{
    const uint32_t diff = resize_selection(state.buffer, cursor.selection, size * times);
    char*          dst  = state.buffer.data + cursor.selection.start;

    for (uint32_t i = 0; i < times; i++, dst += size)
    {
        bx::memCopy(dst, string, size);
    }

    cursor.selection.start =
    cursor.selection.end   =
    cursor.offset          = cursor.selection.start + size * times;

    return diff;
}

static void paste_ex(const Clipboard& clipboard, uint32_t start, uint32_t count, uint32_t size, Array<char>& buffer, Cursor& cursor, uint32_t& offset)
{
    bx::memMove(
        buffer.data + offset + cursor.selection.start + size,
        buffer.data + offset + cursor.selection.end,
        buffer.size - offset - cursor.selection.end
    );

    for (uint32_t i = start, j = cursor.selection.start + offset; i < start + count; i++)
    {
        bx::memCopy(
            buffer.data + j,
            clipboard.buffer.data + clipboard.ranges[i].start,
            range_size(clipboard.ranges[i])
        );

        j += range_size(clipboard.ranges[i]);

        if (i + 1 < start + count && clipboard.buffer[clipboard.ranges[i].end - 1] != '\n')
        {
            buffer[j++] = '\n';
        }
    }

    const uint32_t size_diff = size - range_size(cursor.selection);

    cursor.selection.start = 
    cursor.selection.end   = 
    cursor.offset          = cursor.selection.start + size + offset;

    offset += size_diff;
}

static inline void update_preferred_x(State& state)
{
    for (uint32_t i = 0, line = 0; i < state.cursors.size; i++)
    {
        const Position position = to_position(state, state.cursors[i].selection.start, line);
        ASSERT(position.y >= line);

        state.cursors[i].preferred_x = position.x;

        line = position.y;
    }
}

static void parse_lines(const char* string, Array<Range>& lines)
{
    ASSERT(string);

    lines.clear();
    lines.push_back({});

    uint32_t codepoint;
    uint32_t offset = 1;

    while ((codepoint = utf8_next_codepoint(string)))
    {
        if (codepoint == '\n')
        {
            lines.back().end = offset;
            lines.push_back({ offset });
        }

        offset++;
    }

    lines.back().end = offset;
}

static void paste_multi(State& state, const Clipboard& clipboard)
{
    const bool different_count = clipboard.ranges.size != state.cursors.size;

    uint32_t added   = 0;
    uint32_t removed = 0;

    for (uint32_t i = 0; i < clipboard.ranges.size; i++)
    {
        added += range_size(clipboard.ranges[i]);

        if (different_count && i + 1 < clipboard.ranges.size && clipboard.buffer[clipboard.ranges[i].end - 1] != '\n')
        {
            added++; // For new line character.
        }
    }

    for (uint32_t i = 0; i < state.cursors.size; i++)
    {
        removed += range_size(state.cursors[i].selection);
    }

    if (state.cursors.size != clipboard.ranges.size)
    {
        added *= state.cursors.size;
    }

    if (added > removed)
    {
        state.buffer.resize(state.buffer.size + added - removed);
    }

    for (uint32_t i = 0, offset = 0; i < state.cursors.size; i++)
    {
        if (!different_count)
        {
            paste_ex(clipboard, i, 1, range_size(clipboard.ranges[i]), state.buffer, state.cursors[i], offset);
        }
        else
        {
            paste_ex(clipboard, 0, clipboard.ranges.size, added / state.cursors.size, state.buffer, state.cursors[i], offset);
        }
    }

    parse_lines(state.buffer.data, state.lines);
}

static void remove_cursor(Array<Cursor>& cursors, uint32_t i)
{
    ASSERT(cursors.size > 1);

    for (uint32_t j = i + 1; j < cursors.size; j++)
    {
        cursors[j - 1] = cursors[j];
    }

    cursors.pop_back();
}

static bool remove_cursor_containing_offset(Array<Cursor>& cursors, uint32_t offset)
{
    for (uint32_t i = 0; i < cursors.size; i++)
    {
        if (range_contains(cursors[i].selection, offset))
        {
            if (cursors.size > 1)
            {
                remove_cursor(cursors, i);
            }

            return true;
        }
    }

    return false;
}

static inline void sort_cursors(Array<Cursor>& cursors)
{
    if (cursors.size > 1)
    {
        bx::quickSort(
            cursors.data,
            cursors.size,
            sizeof(Cursor),
            [](const void* first, const void* second) -> int32_t
            {
                return
                    int64_t(reinterpret_cast<const Cursor*>(first )->selection.start) -
                    int64_t(reinterpret_cast<const Cursor*>(second)->selection.start) ;
            }
        );
    }
}

static void fix_overlapping_cursors(Array<Cursor>& cursors)
{
    if (cursors.size < 2)
    {
        return;
    }

    sort_cursors(cursors);

    for (uint32_t i = 1; i < cursors.size; i++)
    {
        Cursor& first  = cursors[i - 1];
        Cursor& second = cursors[i];

        if (first.selection.end >= second.selection.start)
        {
            for (uint32_t j = i + 1; i < cursors.size; i++)
            {
                cursors[j - 1] = cursors[j];
            }

            remove_cursor(cursors, i);
        }
    }
}

static void gather_cursor_ranges(const Array<Cursor>& cursors, const Array<Range>& lines, Array<Range>& ranges)
{
    ranges.clear();

    uint32_t last_copied_line = -1;

    for (uint32_t i = 0; i < cursors.size; i++)
    {
        ASSERT(i == 0 || cursors[i - 1].selection.start < cursors[i].selection.start);

        Range        range = cursors[i].selection;
        const uint32_t line  = to_line(lines, range.end, last_copied_line != -1 ? last_copied_line : 0);

        if (range_empty(range) && line != last_copied_line)
        {
            range = lines[line];
            last_copied_line = line;
        }

        if (!range_empty(range))
        {
            ranges.push_back(range);
        }
    }
}

static void copy_ranges(const Array<char>& src_buffer, const Array<Range>& ranges, Array<char>& dst_buffer)
{
    dst_buffer.clear();

    for (uint32_t i = 0; i < ranges.size; i++)
    {
        ASSERT(!range_empty(ranges[i]));

        const uint32_t size   = range_size(ranges[i]);
        const uint32_t offset = dst_buffer.size;

        dst_buffer.resize(offset + size);
        bx::memCopy(dst_buffer.data + offset, src_buffer.data + ranges[i].start, size);

        if (ranges[i].end == src_buffer.size)
        {
            dst_buffer.back() = '\n';
        }
    }
}

static bool delete_ranges(Array<char>& buffer, const Array<Range>& ranges)
{
    if (!ranges.size)
    {
        return false;
    }

    uint32_t removed = 0;
    uint32_t start   = ranges[0].start;
    uint32_t end     = ranges[0].end;

    for (uint32_t i = 1; i < ranges.size; i++)
    {
        while (i < ranges.size && range_contains({ start, end }, ranges[i].start))
        {
            end = bx::max(end, ranges[i].end);
            i++;
        }

        char*          dst  = buffer.data + start - removed;
        const char*    src  = buffer.data + end   - removed;
        const uint32_t size = buffer.size - end;

        bx::memMove(dst, src, size);
        removed += end - start;

        if (i < ranges.size)
        {
            start = ranges[i].start;
            end   = ranges[i].end;
        }
    }

    buffer.resize(buffer.size - removed);

    ASSERT(buffer.size);
    ASSERT(buffer.back() == 0);

    return true;
}

static void copy_or_move_to_clipboard(State& state, Clipboard& clipboard, bool move)
{
    sort_cursors(state.cursors);

    gather_cursor_ranges(state.cursors, state.lines, clipboard.ranges);
    copy_ranges(state.buffer, clipboard.ranges, clipboard.buffer);

    if (move)
    {
        delete_ranges(state.buffer, clipboard.ranges);
        parse_lines(state.buffer.data, state.lines);
    }

    // Since we're reusing the clipboard range array (to avoid additional heap
    // alloations), we have to adjust the ranges to point into its char buffer.
    for (uint32_t i = 0, offset = 0; i < clipboard.ranges.size; i++)
    {
        const uint32_t size = range_size(clipboard.ranges[i]);

        clipboard.ranges[i] = { offset, offset + size };

        offset += size;
    }
}


// -----------------------------------------------------------------------------
// STATE ACTIONS
// -----------------------------------------------------------------------------

static void action_move_horizontally(State& state, bool left)
{
    for (uint32_t i = 0; i < state.cursors.size; i++)
    {
        Cursor cursor = state.cursors[i];

        if (range_empty(cursor.selection))
        {
            if (left)
            {
                if (cursor.offset > 0)
                {
                    cursor.offset--;
                }
            }
            else if (cursor.offset + 1 < state.buffer.size)
            {
                cursor.offset++;
            }
        }
        else
        {
            cursor.offset = left ? cursor.selection.start : cursor.selection.end;
        }

        cursor.selection.start =
        cursor.selection.end   = cursor.offset;
        cursor.preferred_x     = to_position(state, cursor.offset).x;

        state.cursors[i] = cursor;
    }

    fix_overlapping_cursors(state.cursors);
}

static void action_move_vertically(State& state, bool up)
{
    for (uint32_t i = 0, start_line = 0; i < state.cursors.size; i++)
    {
        Cursor cursor = state.cursors[i];
        uint32_t cursor_line;

        if (!range_empty(cursor.selection))
        {
            const Position position = to_position(state, cursor.selection.start, start_line);

            cursor.preferred_x = position.x;
            cursor_line        =
            start_line         = position.y;
        }
        else
        {
            cursor_line =
            start_line  = to_position(state, cursor.offset, start_line).y;
        }

        if (up)
        {
            if (cursor_line > 0)
            {
                cursor_line--;
            }
        }
        else if (cursor_line + 1 < state.lines.size)
        {
            cursor_line++;
        }

        const uint32_t length = line_length(state, cursor_line);
        ASSERT(length);

        const uint32_t cursor_x = bx::min(cursor.preferred_x, length - 1);

        cursor.selection.start =
        cursor.selection.end   =
        cursor.offset          = to_offset(state, cursor_x, cursor_line);

        state.cursors[i] = cursor;
    }

    fix_overlapping_cursors(state.cursors);
}

static void action_select_horizontally(State& state, bool left)
{
    for (uint32_t i = 0; i < state.cursors.size; i++)
    {
        Cursor    cursor = state.cursors[i];
        uint32_t& stop   = cursor.selection.start == cursor.offset ? cursor.selection.start : cursor.selection.end;

        if (left)
        {
            if (stop > 0)
            {
                stop--;
            }
        }
        else if (stop + 1 < state.buffer.size)
        {
            stop++;
        }

        cursor.offset      = stop;
        cursor.preferred_x = to_position(state, cursor.offset).x;

        range_fix(cursor.selection);

        state.cursors[i] = cursor;
    }

    fix_overlapping_cursors(state.cursors);
}

static void action_select_vertically(State& state, bool up)
{
    for (uint32_t i = 0; i < state.cursors.size; i++)
    {
        Cursor cursor = state.cursors[i];
        uint32_t line   = to_line(state, cursor.offset); // TODO : Figure out `start_line` logic.

        ASSERT(cursor.offset == cursor.selection.start || cursor.offset == cursor.selection.end);

        if (up)
        {
            if (line > 0)
            {
                line--;
            }
            else
            {
                // TODO : Stick to the beginning of the text.
            }
        }
        else if (line + 1 < state.lines.size)
        {
            line++;
        }
        else
        {
            // TODO : Stick to the end of the text.
        }

        const uint32_t cursor_x = bx::min(cursor.preferred_x, line_length(state, line) - 1);
        const uint32_t offset   = to_offset(state, cursor_x, line);

        if (cursor.offset == cursor.selection.end)
        {
            cursor.selection.end =
            cursor.offset        = offset;
        }
        else
        {
            cursor.selection.start =
            cursor.offset          = offset;
        }

        range_fix(cursor.selection);

        state.cursors[i] = cursor;
    }

    fix_overlapping_cursors(state.cursors);
}

static void action_delete(State& state, bool delete_left)
{
    sort_cursors(state.cursors);

    uint32_t removed = 0;

    for (uint32_t i = 0; i < state.cursors.size; i++)
    {
        Cursor& cursor = state.cursors[i];

        if (range_empty(cursor.selection))
        {
            if (delete_left)
            {
                if (cursor.selection.start)
                {
                    cursor.selection.start--;
                }
            }
            else if (cursor.selection.end + 1 < state.buffer.size)
            {
                cursor.selection.end++;
            }
        }

        if (!range_empty(cursor.selection))
        {
            char*        dst  = state.buffer.data + cursor.selection.start - removed;
            const char*  src  = state.buffer.data + cursor.selection.end   - removed;
            const uint32_t size = state.buffer.size - cursor.selection.end;

            bx::memMove(dst, src, size);
            removed += range_size(cursor.selection);

            cursor.selection.start =
            cursor.selection.end   =
            cursor.offset          = dst - state.buffer.data;
            cursor.preferred_x     = to_position(state, cursor.offset).x;
        }
    }

    if (removed)
    {
        state.buffer.resize(state.buffer.size - removed);
        parse_lines(state.buffer.data, state.lines);
    }
}

static void action_cancel_selection(State& state)
{
    state.cursors.resize(1);

    Cursor& cursor = state.cursors[0];

    cursor.selection.start = 0;
    cursor.selection.end   = cursor.offset;
}

static void action_select_all(State& state)
{
    state.cursors.resize(1);

    Cursor& cursor = state.cursors[0];

    cursor.selection.start = 0;
    cursor.selection.end   = 
    cursor.offset          = state.lines.back().end - 1;
}

static bool is_word_separator(uint32_t codepoint, const char* ascii_separators)
{
    for (; *ascii_separators; ascii_separators++)
    {
        if (codepoint == *ascii_separators)
        {
            return true;
        }
    }

    return false;
}

static void action_select_word(State& state)
{
    // NOTE : This should be called right after `click`, so the last cursor in
    //        the array should be the lastly added one.
    Cursor& cursor = state.cursors.back();

    if (!range_empty(cursor.selection))
    {
        return;
    }

    const Range line = state.lines[to_line(state, cursor.offset)];

    if (range_size(line) == 1)
    {
        ASSERT(state.buffer[line.start] == '\n');
        return;
    }

    const char* start = state.buffer.data + cursor.offset;
    const char* end   = start;

    uint32_t   codepoint = utf8_next_codepoint(end);
    const bool separator = is_word_separator(codepoint, state.word_separators);

    while (codepoint && codepoint != '\n' && separator == is_word_separator(codepoint, state.word_separators))
    {
        cursor.selection.end = end - state.buffer.data;
        codepoint = utf8_next_codepoint(end);
    }

    while (cursor.selection.start > line.start)
    {
        codepoint = utf8_prev_codepoint(start);

        if (!codepoint || separator != is_word_separator(codepoint, state.word_separators))
        {
            break;
        }

        cursor.selection.start = start - state.buffer.data;
    }

    cursor.offset = cursor.selection.end;
}

static void action_select_line(State& state)
{
    // NOTE : This should be called right after `click`, so the last cursor in
    //        the array should be the lastly added one.
    Cursor& cursor = state.cursors.back();

    cursor.selection = state.lines[to_line(state, cursor.offset)];
    cursor.offset    = cursor.selection.end;
}

static void action_tab(State& state)
{
    // TODO : Consider reducing the number of allocations (would require two passes).

    sort_cursors(state.cursors);

    for (uint32_t i = 0, line = 0, offset = 0; i < state.cursors.size; i++)
    {
        Cursor& cursor = state.cursors[i];

        if (i)
        {
            cursor.selection.start += offset;
            cursor.selection.end   += offset;
            cursor.offset          += offset;
        }

        line = to_line(state, cursor.selection.start, line);

        // TODO : Line without last character that is `\n`.
        if (range_empty(cursor.selection) || cursor.selection.end < state.lines[line].end)
        {
            const uint32_t x = to_column(state, cursor.selection.start, line);
            const uint32_t n = state.tab_size - (x % state.tab_size);

            paste_string(state, cursor, " ", 1, n);

            offset += n;
        }
        else
        {
            // TODO: Add offset to the beginning of each line intersecting the selection.
        }
    }

    parse_lines(state.buffer.data, state.lines);
}

static void action_clear_history(State& state)
{
    state.history.clear();
}


// -----------------------------------------------------------------------------
// PUBLIC API
// -----------------------------------------------------------------------------

State::State()
{
    clear();

    word_separators = " `~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?"; // TODO : Whitespaces in a separate list?
    tab_size        = 4;
}

void State::clear()
{
    buffer.clear();
    buffer.reserve(4096);
    buffer.push_back(0);

    lines.clear();
    lines.reserve(128);
    lines.push_back({ 0, 1 });

    cursors.clear();
    cursors.reserve(16);
    cursors.push_back({});

    char_width  = 0.0f;
    line_height = 0.0f;

    action(Action::CLEAR_HISTORY);
}

void State::click(float x, float y, bool multi_mode)
{
    const Position position = click_position(*this, x, y);
    const uint32_t   offset   = to_offset(*this, position.x, position.y);
    Cursor*        cursor   = nullptr;

    if (multi_mode)
    {
        if (!remove_cursor_containing_offset(cursors, offset))
        {
            cursors.resize(cursors.size + 1);
            cursor = &cursors.back();
        }
    }
    else
    {
        cursors.resize(1);
        cursor = &cursors.front();
    }

    if (cursor)
    {
        cursor->selection.start =
        cursor->selection.end   =
        cursor->offset          = offset;
        cursor->preferred_x     = position.x;
    }
}

// TODO : Consider adding support for different modes of dragging, similar to
//        VS Code (char / word / line), in conjunction with `Action::SELECT_*`
void State::drag(float x, float y)
{
    // Lastly added cursor is considered the active one to drive the selection.
    const uint32_t active = cursors.size - 1;
    uint32_t       count  = active;

    const Position position = click_position(*this, x, y);
    const uint32_t offset   = to_offset(*this, position.x, position.y);

    Cursor& cursor = cursors[active];

    if (cursor.offset == cursor.selection.end)
    {
        cursor.selection.end =
        cursor.offset        = offset;
    }
    else
    {
        cursor.selection.start =
        cursor.offset          = offset;
    }

    range_fix(cursor.selection);

    const Range selection = cursor.selection;

    for (uint32_t i = 0; i < count; i++)
    {
        if (range_overlap(selection, cursors[i].selection))
        {
            for (uint32_t j = i + 1; j < count; j++)
            {
                cursors[j - 1] = cursors[j];
            }

            count--;
        }
    }

    if (count + 1 != cursors.size)
    {
        bx::swap(cursors[count], cursors[active]);
        cursors.resize(count + 1);
    }
}

void State::action(Action action)
{
    switch (action)
    {
        case Action::MOVE_LEFT:
        case Action::MOVE_RIGHT:
            action_move_horizontally(*this, action == Action::MOVE_LEFT);
            break;

        case Action::MOVE_UP:
        case Action::MOVE_DOWN:
            action_move_vertically(*this, action == Action::MOVE_UP);
            break;

        case Action::SELECT_LEFT:
        case Action::SELECT_RIGHT:
            action_select_horizontally(*this, action == Action::SELECT_LEFT);
            break;

        case Action::SELECT_UP:
        case Action::SELECT_DOWN:
            action_select_vertically(*this, action == Action::SELECT_UP);
            break;

        case Action::DELETE_LEFT:
        case Action::DELETE_RIGHT:
            action_delete(*this, action == Action::DELETE_LEFT);
            break;

        case Action::CANCEL_SELECTION:
            action_cancel_selection(*this);
            break;

        case Action::SELECT_ALL:
            action_select_all(*this);
            break;

        case Action::SELECT_WORD:
            action_select_word(*this);
            break;

        case Action::SELECT_LINE:
            action_select_line(*this);
            break;

        case Action::NEW_LINE:
            paste("\n", 1);
            break;

        case Action::TAB:
            action_tab(*this);
            break;

        case Action::CLEAR_HISTORY:
            action_clear_history(*this);
            break;

        case Action::UNDO:
            // ...
            break;

        case Action::REDO:
            // ...
            break;

        default:
            ASSERT(false && "Not yet implemented.");
    }
}

void State::codepoint(uint32_t codepoint)
{
    char buffer[4] = {};

    paste(buffer, utf8_encode(codepoint, buffer));
}

void State::copy(Clipboard& out_clipboard)
{
    copy_or_move_to_clipboard(*this, out_clipboard, false);
}

void State::cut(Clipboard& out_clipboard)
{
    copy_or_move_to_clipboard(*this, out_clipboard, true);
}

void State::paste(const Clipboard& clipboard)
{
    if (clipboard.ranges.size == 1)
    {
        paste(clipboard.buffer.data + clipboard.ranges[0].start, range_size(clipboard.ranges[0]));
    }
    else if (clipboard.ranges.size)
    {
        sort_cursors(cursors);

        paste_multi(*this, clipboard);
    }
}

void State::paste(const char* string, uint32_t size)
{
    if (!string)
    {
        return;
    }

    if (!size)
    {
        size = utf8_size(string);

        if (!size)
        {
            return;
        }
    }

    sort_cursors(cursors);

    for (uint32_t i = 0, offset = 0; i < cursors.size; i++)
    {
        cursors[i].selection.start += offset;
        cursors[i].selection.end   += offset;
        cursors[i].offset          += offset;

        offset += paste_string(*this, cursors[i], string, size);
    }

    parse_lines(buffer.data, lines);

    update_preferred_x(*this);
}

} // namespace tes

} // namespace mnm
