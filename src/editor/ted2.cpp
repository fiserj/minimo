#include "ted2.h"

#include <string.h>  // memmove

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

static inline bool empty(const Range& range)
{
    return range.start == range.end;
}

static inline size_t size(const Range& range)
{
    return range.end - range.start;
}

static inline bool contains(const Range& range, size_t offset)
{
    return range.start <= offset && range.end + empty(range) > offset;
}

static bool overlap(const Range& first, const Range& second)
{
    return
        (first.start >= second.start && first.end   <= second.end) ||
        (first.start >= second.start && first.start <= second.end) ||
        (first.end   >= second.start && first.end   <= second.end) ;
}

static Range intersection(const Range& first, const Range& second)
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

static inline const char* line(const State& state, size_t line)
{
    return state.buffer.data() + state.lines[line].start;
}

static size_t offset(State& state, size_t x, size_t y)
{
    utf8_int32_t codepoint = 0;
    const void*  iterator  = utf8codepoint(line(state, y), &codepoint);

    while (codepoint && codepoint != '\n' && x--)
    {
        iterator = utf8codepoint(iterator, &codepoint);
    }

    return static_cast<const char*>(iterator) - state.buffer.data();
}

static Position position(State& state, size_t offset)
{
    Position position = {};

    for (size_t i = 0; i < state.lines.size(); i++)
    {
        if (contains(state.lines[i], offset))
        {
            position.y = i;
            position.x = utf8nlen(line(state, i), offset - state.lines[i].start);

            break;
        }
    }

    return position;
}


// -----------------------------------------------------------------------------
// PUBLIC API
// -----------------------------------------------------------------------------

State::State()
{
    buffer .reserve(4096);
    lines  .reserve(128);
    cursors.reserve(16);

    buffer .push_back({});
    lines  .push_back({ 0, 1 });
    cursors.push_back({});

    char_width  = 0.0f;
    line_height = 0.0f;
}


// -----------------------------------------------------------------------------

} // namespace ted
