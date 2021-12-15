#pragma once

#include <stdint.h> // uint32_t
#include <stddef.h> // size_t

#include <vector>   // vector

namespace ted
{

enum struct Action
{
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,

    GO_BACK,
    GO_FORWARD,

    MOVE_LINE_UP,
    MOVE_LINE_DOWN,

    COUNT,
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

struct State
{
    State();

    void click(float x, float y, bool multi_mode);

    void drag(float x, float y, bool multi_mode);

    void action(Action action);

    void codepoint(uint32_t codepoint);

    void cut();

    void paste(const char* string, size_t length = 0);

    std::vector<char>   buffer;
    std::vector<Range>  lines;
    std::vector<Cursor> cursors;
    float               char_width;
    float               line_height;
};

} // namespace ted
