#include "ted.h"

#include <assert.h>  // assert
#include <string.h>  // memcpy, memmove

#include <algorithm> // min/max, sort, swap

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

static inline void range_fix(Range& range)
{
    if (range.end < range.start)
    {
        std::swap(range.start, range.end);
    }
}

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

static bool range_overlap(const Range& first, const Range& second)
{
    return
        (first.start >= second.start && first.end   <= second.end) ||
        (first.start >= second.start && first.start <= second.end) ||
        (first.end   >= second.start && first.end   <= second.end) ;
}

// static Range range_intersection(const Range& first, const Range& second)
// {
//     Range range;
//     range.start = std::max(first.start, second.start);
//     range.end   = std::min(first.end  , second.end  );

//     if (range.start > range.end)
//     {
//         range.start = 0.0f;
//         range.end   = 0.0f;
//     }

//     return range;
// }

static inline const char* line_string(const State& state, size_t line)
{
    return state.buffer.data() + state.lines[line].start;
}

static inline size_t line_length(const State& state, size_t line)
{
    return utf8nlen(line_string(state, line), range_size(state.lines[line]));
}

static size_t to_line(const Array<Range>& lines, size_t offset, size_t start_line = 0)
{
    for (size_t i = start_line; i < lines.size(); i++)
    {
        if (range_contains(lines[i], offset))
        {
            return i;
        }
    }

    return 0;
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
    Position position = { 0, 0 };

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

static Position click_position(const State& state, float x, float y)
{
    x = std::max(x, 0.0f) / state.line_height;
    y = std::max(y, 0.0f) / state.char_width;

    const size_t yi = std::min(static_cast<size_t>(y), state.lines.size() - 1);
    const size_t xi = std::min(static_cast<size_t>(x + 0.5f), line_length(state, yi) - 1);

    return { xi, yi };
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

// Needed since `drag` can produce cursor with selection having `end < start`.
static inline void fix_last_cursor(Array<Cursor>& cursors)
{
    range_fix(cursors[cursors.size() - 1].selection);
}

static inline void sort_cursors(Array<Cursor>& cursors)
{
    if (cursors.size() > 1)
    {
        std::sort(
            cursors.data(),
            cursors.data() + cursors.size(),
            [](const Cursor& first, const Cursor& second)
            {
                return first.selection.start < second.selection.start;
            }
        );
    }
}

static void fix_overlapping_cursors(Array<Cursor>& cursors)
{
    if (cursors.size() < 2)
    {
        return;
    }

    sort_cursors(cursors);

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

static void move_cursors_horizontally(State& state, bool left)
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

    fix_overlapping_cursors(state.cursors);
}

static void move_cursors_vertically(State& state, bool up)
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

    fix_overlapping_cursors(state.cursors);
}

static void select_all(State& state)
{
    state.cursors.resize(1);

    Cursor& cursor = state.cursors[0];

    cursor.selection.start = 0;
    cursor.selection.end   = 
    cursor.offset          = state.lines[state.lines.size() - 1].end;
}

static void add_to_clipboard(Clipboard& clipboard, const Array<char>& buffer, const Range& selection)
{
    assert(!range_empty(selection));

    const size_t size   = range_size(selection);
    const size_t offset = clipboard.buffer.size();

    clipboard.buffer.resize(offset + size);
    memcpy(clipboard.buffer.data() + offset, buffer.data() + selection.start, size);

    if (selection.end == buffer.size())
    {
        clipboard.buffer[clipboard.buffer.size() - 1] = '\n';
    }

    clipboard.ranges.push_back({offset, offset + size });
}

static void copy_to_clipboard(State& state, Clipboard& clipboard, Array<Range>* copied_selections = nullptr)
{
    clipboard.buffer.clear();
    clipboard.ranges.clear();

    fix_last_cursor(state.cursors);
    sort_cursors(state.cursors);

    size_t last_copied_line = -1;

    for (size_t i = 0; i < state.cursors.size(); i++)
    {
        Range        selection = state.cursors[i].selection;
        const size_t line      = to_line(state.lines, selection.end, last_copied_line != -1 ? last_copied_line : 0);

        if (range_empty(selection) && line != last_copied_line)
        {
            selection = state.lines[line];
            last_copied_line = line;
        }

        if (!range_empty(selection))
        {
            add_to_clipboard(clipboard, state.buffer, selection);

            if (copied_selections)
            {
                copied_selections->push_back(selection);
            }
        }
    }
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
    fix_last_cursor(cursors);

    const Position position = click_position(*this, x, y);
    const size_t   offset   = to_offset(*this, position.x, position.y);
    Cursor*        cursor   = nullptr;

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
        cursor->preferred_x     = position.x;
    }
}

void State::drag(float x, float y)
{
    // Lastly added cursor is considered the active one to drive the selection.
    const size_t active = cursors.size() - 1;
    size_t       count  = active;

    const Position position = click_position(*this, x, y);
    const size_t   offset   = to_offset(*this, position.x, position.y);

    Cursor& cursor = cursors[active];
    cursor.selection.end =
    cursor.offset        = offset; // This means `end` may be smaller than `start`.

    const Range selection =
    {
        std::min(cursor.selection.start, cursor.selection.end),
        std::max(cursor.selection.start, cursor.selection.end),
    };

    for (size_t i = 0; i < count; i++)
    {
        if (range_overlap(selection, cursors[i].selection))
        {
            for (size_t j = i + 1; j < count; j++)
            {
                cursors[j - 1] = cursors[j];
            }

            count--;
        }
    }

    if (count + 1 != cursors.size())
    {
        std::swap(cursors[count], cursors[active]);
        cursors.resize(count + 1);
    }
}

void State::action(Action action)
{
    fix_last_cursor(cursors);

    switch (action)
    {
        case Action::MOVE_LEFT:
        case Action::MOVE_RIGHT:
            move_cursors_horizontally(*this, action == Action::MOVE_LEFT);
            break;

        case Action::MOVE_UP:
        case Action::MOVE_DOWN:
            move_cursors_vertically(*this, action == Action::MOVE_UP);
            break;

        case Action::SELECT_ALL:
            select_all(*this);
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

void State::copy(Clipboard& out_clipboard)
{
    copy_to_clipboard(*this, out_clipboard);
}

void State::cut(Clipboard& out_clipboard)
{
    // TODO : Try to eliminate this additional heap-allocated resource (will
    //        need to store the absolute ranges first, and adjust them to the
    //        in-clipboard-buffer ones while deleting the selections.
    Array<Range> selections;
    copy_to_clipboard(*this, out_clipboard, &selections);

    assert(selections.size());

    size_t removed = 0;
    size_t start   = selections[0].start;
    size_t end     = selections[0].end;

    for (size_t i = 1; i < selections.size(); i++)
    {
        while (i < selections.size() && range_contains({ start, end }, selections[i].start))
        {
            end = std::max(end, selections[i].end);
            i++;
        }

        char*        dst  = buffer.data() + start - removed;
        const char*  src  = buffer.data() + end   - removed;
        const size_t size = buffer.size() - end;

        memmove(dst, src, size);
        removed += end - start;

        if (i < selections.size())
        {
            start = selections[i].start;
            end   = selections[i].end;
        }
    }

    buffer.resize(buffer.size() - removed);
    assert(buffer.size());
    assert(buffer[buffer.size() - 1] == 0);

    parse_lines(buffer.data(), lines);
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

    fix_last_cursor(cursors);

    for (size_t i = 0; i < cursors.size(); i++)
    {
        if (const size_t shift = paste_at(*this, cursors[i], string, size))
        {
            for (size_t j = i + 1; j < cursors.size(); j++)
            {
                cursors[j].selection.start += shift;
                cursors[j].selection.end   += shift;
                cursors[j].offset          += shift;
                // TODO : Preferred X.
            }
        }
    }

    parse_lines(buffer.data(), lines);
}


// -----------------------------------------------------------------------------
// TESTS
// -----------------------------------------------------------------------------

#if defined(TED_TESTS) && !defined(NDEBUG)

namespace tests
{

struct TestState : State
{
    TestState()
    {
        char_width  = 10.0f;
        line_height = 20.0f;

        check_invariants();

        // https://en.wikipedia.org/wiki/Salamander
        paste(
            "Salamanders are a group of amphibians typically characterized by\n"
            "their lizard-like appearance, with slender bodies, blunt snouts,\n"
            "short limbs projecting at right angles to the body, and the presence\n"
            "of a tail in both larvae and adults.\n"
            "\n"
            "All ten extant salamander families are grouped together under the\n"
            "order Urodela.\n"
            "\n"
            "Salamander diversity is highest in the Northern Hemisphere and most\n"
            "species are found in the Holarctic realm, with some species present\n"
            "in the Neotropical realm."
        );

        check_invariants();

        check_size(481);

        check_line_count(11);

        check_cursor_count(1);
        check_cursor(0, { { 480, 480 }, 480, 0 }); // TODO : Preferred X.        
    }

    void check_invariants()
    {
        assert(buffer.size());
        assert(buffer[buffer.size() - 1] == 0);

        assert(lines.size());
        assert(lines[0].start == 0);
        assert(lines[lines.size() - 1].end == buffer.size());

        assert(cursors.size());
    }

    void check_size(size_t expected)
    {
        assert(buffer.size() == expected);
    }

    void check_string(const char* expected)
    {
        assert(0 == utf8cmp(buffer.data(), expected));
    }

    void check_line_count(size_t expected)
    {
        assert(lines.size() == expected);
    }

    void check_cursor_count(size_t expected)
    {
        assert(cursors.size() == expected);
    }

    void check_cursor(size_t index, const Cursor& cursor)
    {
        assert(index < cursors.size());

        assert(cursor.selection.start <= cursor.selection.end);

        assert(cursors[index].selection.start == cursor.selection.start);
        assert(cursors[index].selection.end   == cursor.selection.end  );
        assert(cursors[index].offset          == cursor.offset         );
        assert(cursors[index].preferred_x     == cursor.preferred_x    );
    }
};

struct TestClipboard : Clipboard
{
    void check_size(size_t expected)
    {
        assert(ranges.size() == expected);
    }

    void check_string(size_t index, const char* expected)
    {
        assert(index < ranges.size());
        assert(0 == utf8ncmp(
            buffer.data() + ranges[index].start,
            expected,
            range_size(ranges[index])
        ));
    }
};

static void test_cut()
{
    TestState state;

    state.cursors.clear();
    state.cursors.push_back({ {  16,  38 } }); // "a group of amphibians ".
    state.cursors.push_back({ {  70,  70 } }); // 2nd line, including `\n`.
    state.cursors.push_back({ { 100, 107 } }); // "slender".
    state.cursors.push_back({ { 110, 110 } }); // 2nd line again, this time skipped.
    state.cursors.push_back({ { 299, 308 } }); // "the\norder" (spans two lines).
    state.cursors.push_back({ { 315, 315 } }); // 7th line.

    TestClipboard clipboard;
    state.cut(clipboard);

    state.check_invariants();

    clipboard.check_size(5);
    clipboard.check_string(0, "a group of amphibians ");
    clipboard.check_string(1, "their lizard-like appearance, with slender bodies, blunt snouts,\n");
    clipboard.check_string(2, "slender");
    clipboard.check_string(3, "the\norder");

    state.check_line_count(8);
    state.check_size(375);
    state.check_string(
        "Salamanders are typically characterized by\n"
        "short limbs projecting at right angles to the body, and the presence\n"
        "of a tail in both larvae and adults.\n"
        "\n"
        "All ten extant salamander families are grouped together under \n"
        "Salamander diversity is highest in the Northern Hemisphere and most\n"
        "species are found in the Holarctic realm, with some species present\n"
        "in the Neotropical realm."
    );
}

static bool s_tests_done = []()
{
    test_cut();

    return true;
}();

} // namespace tests

#endif // defined(TED_TESTS) && !defined(NDEBUG)


// -----------------------------------------------------------------------------

} // namespace ted
