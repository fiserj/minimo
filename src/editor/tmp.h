#pragma once

namespace ted
{

constexpr int MAX_BUFFER_SIZE = 1 << 20;

constexpr int MAX_CURSORS     = 1 << 10;

constexpr int MAX_LINE_COUNT  = 1 << 12;

struct Range
{
    int start;
    int end; // "One past".

    inline bool is_empty() const
    {
        return start == end;
    }

    inline bool overlaps(const Range& other) const
    {
        return
            (start >= other.start && end   <= other.end) ||
            (start >= other.start && start <= other.end) ||
            (end   >= other.start && end   <= other.end) ;
    }

    inline Range intersect(Range other) const
    {
        return { bx::max(start, other.start), bx::min(end, other.end) };
    }
};

struct Position
{
    int x; // Character.
    int y; // Line.
};

struct Cursor
{
    Range selection;
    int   offset;
    int   preferred_x;
};

struct Document
{
    char  buffer[MAX_BUFFER_SIZE]; // Includes terminating '\0'.
    int   buffer_size;

    Range lines[MAX_LINE_COUNT];
    int   line_count;

    Cursor cursors[MAX_CURSORS];
    int    cursor_count;

    float char_width;
    float line_height;

    void set_content(const char* string)
    {
        bx::memSet(this, 0, sizeof(*this));

        if (!string || !*string)
        {
            buffer_size  = 1;
            lines[0].end = 1;
            line_count   = 1;

            return;
        }

        for (; *string && buffer_size < MAX_BUFFER_SIZE - 1; string++)
        {
            buffer[buffer_size++] = *string;

            if (*string == '\n')
            {
                lines[line_count++].end   = buffer_size;
                lines[line_count  ].start = buffer_size;
            }
        }

        lines[line_count].end = ++buffer_size; // Final null terminator.
    }

    Position get_position(int offset, int line = 0) const
    {
        Position position = {};

        for (; line < line_count; line++)
        {
            if (offset >= lines[line].start && offset < lines[line].end)
            {
                position.x = utf8nlen(&buffer[lines[line].start], offset - lines[line].start);
                position.y = line;
                break;
            }
        }

        return position;
    }

    void click(float x, float y, bool add_mode = false)
    {
        const int yi = (int)bx::min(line_count - 1.0f, y / line_height);
        const int xi = ;


        if (!add_mode)
        {

        }
    }

    void merge_cursors()
    {
        for (int i = 0; i < cursor_count; i++)
        {
            // ...
        }
    }
};

} // namespace ted
