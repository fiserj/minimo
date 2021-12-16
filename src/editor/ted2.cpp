#include "ted2.h"

#include <string.h>  // memcpy, memmove

#include <algorithm> // min/max

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

static Position to_position(State& state, size_t offset)
{
    Position position = {};

    for (size_t i = 0; i < state.lines.size(); i++)
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

static size_t paste_at(std::vector<char>& buffer, Cursor& cursor, const char* string, size_t size)
{
    const size_t selection = range_size(cursor.selection);

    if (size != selection)
    {
        const size_t src  = cursor.selection.end;
        const size_t dst  = cursor.selection.start + size;
        const size_t span = buffer.size() - src;

        if (size > selection)
        {
            buffer.resize(buffer.size() + size - selection);
        }

        memmove(buffer.data() + dst, buffer.data() + src, span);
    }

    memcpy(buffer.data() + cursor.selection.start, string, size);

    cursor.selection.start =
    cursor.selection.end   =
    cursor.offset          = cursor.selection.start + size;

    return 0;
}

static void parse_lines(const char* string, std::vector<Range>& out_lines)
{
    assert(string);

    out_lines.clear(); // Do really all major implementations keep the memory?
    out_lines.push_back({});

    utf8_int32_t codepoint = 0;
    const void*  iterator  = utf8codepoint(string, &codepoint);
    size_t       offset    = 1;

    while (codepoint)
    {
        if (codepoint == '\n')
        {
            out_lines.back().end = offset;
            out_lines.push_back({ offset });
        }

        iterator = utf8codepoint(iterator, &codepoint);
        offset++;
    }

    out_lines.back().end = offset;
}

// static void sanitize_cursors(std::vector<Cursor>& cursors)
// {
//     assert(false && "TODO");
// }


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
        if (const size_t shift = paste_at(buffer, cursors[i], string, size))
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


// -----------------------------------------------------------------------------
// TESTS
// -----------------------------------------------------------------------------

#if 1

#include <assert.h>

static bool s_tests_done = []()
{
    State state;
    assert(state.buffer .size() == 1);
    assert(state.lines  .size() == 1);
    assert(state.cursors.size() == 1);
    assert(state.buffer .back() == 0);

    state.paste("One\ntwo");
    assert(state.buffer .size() == 9);
    assert(state.lines  .size() == 1);
    assert(state.cursors.size() == 1);
    assert(state.buffer .back() == 0);

    return true;
}();

#endif // 1


// -----------------------------------------------------------------------------

} // namespace ted
