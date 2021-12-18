#include "ted.h"

#include <assert.h>  // assert
#include <string.h>  // memcpy, memmove

#include <algorithm> // min/max, sort

#include <utf8.h>    // utf8*

namespace ted
{

// -----------------------------------------------------------------------------
// INTERNAL HELPERS
// -----------------------------------------------------------------------------

struct Position
{
    size_t x;
    size_t y;
};

static inline bool range_empty(const Range& range)
{
    return range.start == range.end;
}

static inline size_t range_size(const Range& range)
{
    return range.end - range.start;
}

static inline bool range_contains(const Range& range, size_t offset)
{
    return range.start <= offset && range.end + range_empty(range) > offset;
}

// static bool range_overlap(const Range& first, const Range& second)
// {
//     return
//         (first.start >= second.start && first.end   <= second.end) ||
//         (first.start >= second.start && first.start <= second.end) ||
//         (first.end   >= second.start && first.end   <= second.end) ;
// }

static Range range_intersection(const Range& first, const Range& second)
{
    Range range;
    range.start = std::max(first.start, second.start);
    range.end   = std::min(first.end  , second.end  );

    if (range.start > range.end)
    {
        range.start = 0.0f;
        range.end   = 0.0f;
    }

    return range;
}

static inline const char* line_string(const State& state, size_t line)
{
    return state.buffer.data() + state.lines[line].start;
}

static inline size_t line_length(const State& state, size_t line)
{
    return utf8nlen(line_string(state, line), range_size(state.lines[line]));
}

static size_t to_offset(State& state, size_t x, size_t y)
{
    utf8_int32_t codepoint = 0;
    const void*  iterator  = utf8codepoint(line_string(state, y), &codepoint);

    while (codepoint && codepoint != '\n' && x--)
    {
        iterator = utf8codepoint(iterator, &codepoint);
    }

    return static_cast<const char*>(iterator) - state.buffer.data();
}

static Position to_position(State& state, size_t offset, size_t start_line = 0)
{
    Position position = {};

    for (size_t i = start_line; i < state.lines.size(); i++)
    {
        if (range_contains(state.lines[i], offset))
        {
            position.y = i;
            position.x = utf8nlen(line_string(state, i), offset - state.lines[i].start);

            break;
        }
    }

    return position;
}

static size_t paste_at(State& state, Cursor& cursor, const char* string, size_t size)
{
    const size_t selection = range_size(cursor.selection);

    if (size != selection)
    {
        const size_t src  = cursor.selection.end;
        const size_t dst  = cursor.selection.start + size;
        const size_t span = state.buffer.size() - src;

        if (size > selection)
        {
            state.buffer.resize(state.buffer.size() + size - selection);
        }

        memmove(state.buffer.data() + dst, state.buffer.data() + src, span);
    }

    memcpy(state.buffer.data() + cursor.selection.start, string, size);

    cursor.selection.start =
    cursor.selection.end   =
    cursor.offset          = cursor.selection.start + size;
    cursor.preferred_x     = to_position(state, cursor.offset).x;

    return 0;
}

static void parse_lines(const char* string, Array<Range>& lines)
{
    assert(string);

    lines.clear(); // Do really all major implementations keep the memory?
    lines.push_back({});

    utf8_int32_t codepoint = 0;
    const void*  iterator  = utf8codepoint(string, &codepoint);
    size_t       offset    = 1;

    while (codepoint)
    {
        if (codepoint == '\n')
        {
            lines[lines.size() - 1].end = offset;
            lines.push_back({ offset });
        }

        iterator = utf8codepoint(iterator, &codepoint);
        offset++;
    }

    lines[lines.size() - 1].end = offset;
}

static void remove_cursor(Array<Cursor>& cursors, size_t i)
{
    assert(cursors.size() > 1);

    for (size_t j = i + 1; j < cursors.size(); j++)
    {
        cursors[j - 1] = cursors[j];
    }

    cursors.resize(cursors.size() - 1);
}

static bool remove_cursor_containing_offset(Array<Cursor>& cursors, size_t offset)
{
    for (size_t i = 0; i < cursors.size(); i++)
    {
        if (range_contains(cursors[i].selection, offset))
        {
            if (cursors.size() > 1)
            {
                remove_cursor(cursors, i);
            }

            return true;
        }
    }

    return false;
}

static void sanitize_cursors(Array<Cursor>& cursors)
{
    if (cursors.size() < 2)
    {
        return;
    }

    std::sort(
        cursors.data(),
        cursors.data() + cursors.size(),
        [](const Cursor& first, const Cursor& second)
        {
            return first.selection.start < second.selection.start;
        }
    );

    for (size_t i = 1; i < cursors.size(); i++)
    {
        Cursor& first  = cursors[i - 1];
        Cursor& second = cursors[i];

        if (first.selection.end >= second.selection.start)
        {
            assert(!range_empty(first .selection));
            assert(!range_empty(second.selection));

            for (size_t j = i + 1; i < cursors.size(); i++)
            {
                cursors[j - 1] = cursors[j];
            }

            remove_cursor(cursors, i);
        }
    }
}

static void move_cursor_horizontally(State& state, bool left)
{
    for (size_t i = 0; i < state.cursors.size(); i++)
    {
        Cursor& cursor = state.cursors[i];

        if (range_empty(cursor.selection))
        {
            if (left)
            {
                if (cursor.offset > 0)
                {
                    cursor.offset--;
                }
            }
            else if (cursor.offset + 1 < state.buffer.size())
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
    }

    sanitize_cursors(state.cursors);
}

static void move_cursor_vertically(State& state, bool up)
{
    for (size_t i = 0, start_line = 0; i < state.cursors.size(); i++)
    {
        Cursor& cursor = state.cursors[i];
        size_t  cursor_line;

        if (!range_empty(cursor.selection))
        {
            const size_t   offset   = up ? cursor.selection.start : cursor.selection.end;
            const Position position = to_position(state, offset, start_line);

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
        else if (cursor_line + 1 < state.lines.size())
        {
            cursor_line++;
        }

        const size_t length = line_length(state, cursor_line);
        assert(length);

        const size_t cursor_x = std::min(cursor.preferred_x, length - 1);

        cursor.selection.start =
        cursor.selection.end   =
        cursor.offset          = to_offset(state, cursor_x, cursor_line);
    }

    sanitize_cursors(state.cursors);
}


// -----------------------------------------------------------------------------
// PUBLIC API
// -----------------------------------------------------------------------------

State::State()
{
    clear();
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
}

void State::click(float x, float y, bool multi_mode)
{
    x = std::max(x, 0.0f);
    y = std::max(y, 0.0f);

    const size_t yi = std::min(static_cast<size_t>(y / line_height), lines.size() - 1);
    const size_t xi = std::min(static_cast<size_t>(x / char_width + 0.5f), line_length(*this, yi) - 1);

    const size_t offset = to_offset(*this, x, y);
    Cursor*      cursor = nullptr;

    if (multi_mode)
    {
        if (!remove_cursor_containing_offset(cursors, offset))
        {
            cursors.resize(cursors.size() + 1);
            cursor = &cursors[cursors.size() - 1];
        }
    }
    else
    {
        cursors.resize(1);
        cursor = &cursors[0];
    }

    if (cursor)
    {
        cursor->selection.start =
        cursor->selection.end   =
        cursor->offset          = offset;
        cursor->preferred_x     = xi;
    }
}

void State::paste(const char* string, size_t size)
{
    if (!string)
    {
        return;
    }

    if (!size)
    {
        size = utf8size_lazy(string);

        if (!size)
        {
            return;
        }
    }

    for (size_t i = 0; i < cursors.size(); i++)
    {
        if (const size_t shift = paste_at(*this, cursors[i], string, size))
        {
            for (size_t j = i + 1; j < cursors.size(); j++)
            {
                cursors[j].selection.start += shift;
                cursors[j].selection.end   += shift;
                cursors[j].offset          += shift;
            }
        }
    }

    parse_lines(buffer.data(), lines);
}

void State::action(Action action)
{
    switch (action)
    {
        case Action::MOVE_LEFT:
        case Action::MOVE_RIGHT:
            move_cursor_horizontally(*this, action == Action::MOVE_LEFT);
            break;

        case Action::MOVE_UP:
        case Action::MOVE_DOWN:
            move_cursor_vertically(*this, action == Action::MOVE_UP);
            break;

        default:
            assert(false && "Not yet implemented.");
    }
}

void State::codepoint(uint32_t codepoint)
{
    if (!utf8nvalid(&codepoint, 4))
    {
        paste(reinterpret_cast<const char*>(&codepoint));
    }
}


// -----------------------------------------------------------------------------
// TESTS
// -----------------------------------------------------------------------------

#if 0

#include <assert.h>

static bool s_tests_done = []()
{
    State state;
    assert(state.buffer .size() == 1);
    assert(state.lines  .size() == 1);
    assert(state.cursors.size() == 1);
    assert(state.buffer .back() == 0);

    state.paste("One\ntwo\nthree");
    assert(state.buffer .size() == 14);
    assert(state.lines  .size() == 3);
    assert(state.cursors.size() == 1);
    assert(state.buffer .back() == 0);
    assert(state.cursors[0].offset == 13);

    state.cursors[0].selection.start = 4;
    state.cursors[0].selection.end   =
    state.cursors[0].offset          =
    state.cursors[0].preferred_x     = 7;

    state.paste("four\nfive\n");
    assert(state.buffer .size() == 21);
    assert(state.lines  .size() == 5);
    assert(state.cursors.size() == 1);
    assert(state.buffer .back() == 0);
    assert(state.cursors[0].offset == 14);

    state.codepoint('a');
    state.codepoint('b');
    state.codepoint('c');

    return true;
}();

#endif // 0


// -----------------------------------------------------------------------------

} // namespace ted
