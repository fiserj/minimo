#pragma once

#include <stdint.h>  // uint32_t
#include <stddef.h>  // size_t

// `TED_ARRAY` has to implement following subset of `std::vector`'s API:
// `clear`, `data`, `operator[]`, `push_back`, `reserve`, `resize`, `size`.
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
    CANCEL_SELECTION,
    CLEAR,
    CLICK,
    CLICK_MULTI,
    CODEPOINT,
    COPY,
    CUT,
    DELETE_LEFT,
    DELETE_RIGHT,
    DRAG,
    DRAG_MULTI,
    GO_BACK,
    GO_FORWARD,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_LINE_DOWN,
    MOVE_LINE_UP,
    MOVE_RIGHT,
    MOVE_UP,
    PASTE,
    SELECT_ALL,
    SELECT_DOWN,
    SELECT_LEFT,
    SELECT_RIGHT,
    SELECT_UP,
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

    // Use I/O members to set up data for actions that need them.
    void action(Action action);

    // Data storage.
    Array<char>     buffer;
    Array<Range>    lines;
    Array<Cursor>   cursors;

    // I/O.
    Clipboard*      clipboard;
    const char*     utf8_string;
    size_t          utf8_string_bytes;
    const uint32_t* utf8_codepoints;
    size_t          utf8_codepoints_count;
    float           char_width;
    float           line_height;
    float           mouse_x;
    float           mouse_y;

    // Behavior tweaks.
    bool            clear_consumed_input;
};

} // namespace ted
