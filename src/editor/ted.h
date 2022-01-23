#pragma once

#include <stdint.h>  // uint32_t
#include <stddef.h>  // size_t

// `TED_ARRAY` has to implement following subset of `std::vector`'s API:
// `clear`, `data`, `operator[]`, `push_back`, `resize`, `size`.
#ifndef TED_ARRAY
#   include <vector> // vector
#   define TED_ARRAY std::vector
#endif

namespace ted
{

template <typename T>
using Array = TED_ARRAY<T>;

enum struct Action
{
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

    NEW_LINE,

    TAB,

};

struct Range
{
    size_t start;
    size_t end;
};

struct Cursor
{
    Range  selection;
    size_t offset;
    size_t preferred_x;
};

struct Clipboard
{
    Array<char>  buffer;
    Array<Range> ranges;
};

struct State
{
    State();

    void clear();

    void click(float x, float y, bool multi_mode);

    void drag(float x, float y);

    void action(Action action);

    void codepoint(uint32_t codepoint);

    void copy(Clipboard& out_clipboard);

    void cut(Clipboard& out_clipboard);

    void paste(const Clipboard& clipboard);

    void paste(const char* string, size_t size = 0);

    Array<char>   buffer;
    Array<Range>  lines;
    Array<Cursor> cursors;
    const char*   word_separators;
    float         char_width;
    float         line_height;
    size_t        tab_size;
};

} // namespace ted
