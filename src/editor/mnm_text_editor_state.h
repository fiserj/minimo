#pragma once

#include <stdint.h>    // uint*_t

#include <mnm_array.h> // Array

namespace mnm
{

namespace tes
{

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
    SELECT_LINE,

    NEW_LINE,

    TAB,

    CLEAR_HISTORY,
    UNDO,
    REDO,

    _COUNT,
};

struct Range
{
    uint32_t start;
    uint32_t end;
};

struct Cursor
{
    Range    selection;
    uint32_t offset;
    uint32_t preferred_x;
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

    void paste(const char* string, uint32_t size = 0);

    Array<char>    buffer;
    Array<Range>   lines;
    Array<Cursor>  cursors;
    Array<uint8_t> history;
    const char*    word_separators;
    float          char_width;
    float          line_height;
    uint32_t       tab_size;
};

} // namespace tes

} // namespace mnm
