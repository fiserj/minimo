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

static size_t to_offset(const State& state, size_t x, size_t y)
{
    utf8_int32_t codepoint;
    const void*  iterator = utf8codepoint(line_string(state, y), &codepoint);
    size_t       offset   = state.lines[y].start;

    for (; codepoint && codepoint != '\n' && x--; iterator = utf8codepoint(iterator, &codepoint))
    {
        // TODO : This could be provided by a tweaked version of `utf8codepoint`, but low impact / priority.
        offset += utf8codepointsize(codepoint);
    }

    return offset;
}

static size_t to_line(const State& state, size_t offset, size_t start_line = 0)
{
    for (size_t i = start_line; i < state.lines.size(); i++)
    {
        if (range_contains(state.lines[i], offset))
        {
            return i;
        }
    }

    return 0;
}

static Position to_position(const State& state, size_t offset, size_t start_line = 0)
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

static inline size_t to_column(const State& state, size_t offset, size_t line)
{
    assert(range_contains(state.lines[line], offset));

    return utf8nlen(line_string(state, line), offset - state.lines[line].start);
}

static Position click_position(const State& state, float x, float y)
{
    x = std::max(x, 0.0f) / state.char_width;
    y = std::max(y, 0.0f) / state.line_height;

    const size_t yi = std::min(static_cast<size_t>(y), state.lines.size() - 1);
    const size_t xi = std::min(static_cast<size_t>(x + 0.5f), line_length(state, yi) - 1);

    return { xi, yi };
}

static size_t resize_selection(Array<char>& buffer, const Range& selection, size_t new_size)
{
    const size_t old_size = range_size(selection);

    if (new_size != old_size)
    {
        const size_t src  = selection.end;
        const size_t dst  = selection.start + new_size;
        const size_t span = buffer.size() - src;

        if (new_size > old_size)
        {
            buffer.resize(buffer.size() + new_size - old_size);
        }

        memmove(buffer.data() + dst, buffer.data() + src, span);
    }

    return new_size - old_size;
}

static size_t paste_string(State& state, Cursor& cursor, const char* string, size_t size, size_t times = 1)
{
    const size_t diff = resize_selection(state.buffer, cursor.selection, size * times);
    char*        dst  = state.buffer.data() + cursor.selection.start;

    for (size_t i = 0; i < times; i++, dst += size)
    {
        memcpy(dst, string, size);
    }

    cursor.selection.start =
    cursor.selection.end   =
    cursor.offset          = cursor.selection.start + size * times;

    return diff;
}

static void paste_ex(const Clipboard& clipboard, size_t start, size_t count, size_t size, Array<char>& buffer, Cursor& cursor, size_t& offset)
{
    memmove(
        buffer.data() + offset + cursor.selection.start + size,
        buffer.data() + offset + cursor.selection.end,
        buffer.size() - offset - cursor.selection.end
    );

    for (size_t i = start, j = cursor.selection.start + offset; i < start + count; i++)
    {
        memcpy(
            buffer.data() + j,
            clipboard.buffer.data() + clipboard.ranges[i].start,
            range_size(clipboard.ranges[i])
        );

        j += range_size(clipboard.ranges[i]);

        if (i + 1 < start + count && clipboard.buffer[clipboard.ranges[i].end - 1] != '\n')
        {
            buffer[j++] = '\n';
        }
    }

    const size_t size_diff = size - range_size(cursor.selection);

    cursor.selection.start = 
    cursor.selection.end   = 
    cursor.offset          = cursor.selection.start + size + offset;

    offset += size_diff;
}

static inline void update_preferred_x(State& state)
{
    for (size_t i = 0, line = 0; i < state.cursors.size(); i++)
    {
        const Position position = to_position(state, state.cursors[i].selection.start, line);
        assert(position.y >= line);

        state.cursors[i].preferred_x = position.x;

        line = position.y;
    }
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

static void paste_multi(State& state, const Clipboard& clipboard)
{
    const bool different_count = clipboard.ranges.size() != state.cursors.size();

    size_t added   = 0;
    size_t removed = 0;

    for (size_t i = 0; i < clipboard.ranges.size(); i++)
    {
        added += range_size(clipboard.ranges[i]);

        if (different_count && i + 1 < clipboard.ranges.size() && clipboard.buffer[clipboard.ranges[i].end - 1] != '\n')
        {
            added++; // For new line character.
        }
    }

    for (size_t i = 0; i < state.cursors.size(); i++)
    {
        removed += range_size(state.cursors[i].selection);
    }

    if (state.cursors.size() != clipboard.ranges.size())
    {
        added *= state.cursors.size();
    }

    if (added > removed)
    {
        state.buffer.resize(state.buffer.size() + added - removed);
    }

    for (size_t i = 0, offset = 0; i < state.cursors.size(); i++)
    {
        if (!different_count)
        {
            paste_ex(clipboard, i, 1, range_size(clipboard.ranges[i]), state.buffer, state.cursors[i], offset);
        }
        else
        {
            paste_ex(clipboard, 0, clipboard.ranges.size(), added / state.cursors.size(), state.buffer, state.cursors[i], offset);
        }
    }

    parse_lines(state.buffer.data(), state.lines);
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
            for (size_t j = i + 1; i < cursors.size(); i++)
            {
                cursors[j - 1] = cursors[j];
            }

            remove_cursor(cursors, i);
        }
    }
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

static void gather_cursor_ranges(const Array<Cursor>& cursors, const Array<Range>& lines, Array<Range>& ranges)
{
    ranges.clear();

    size_t last_copied_line = -1;

    for (size_t i = 0; i < cursors.size(); i++)
    {
        assert(i == 0 || cursors[i - 1].selection.start < cursors[i].selection.start);

        Range        range = cursors[i].selection;
        const size_t line  = to_line(lines, range.end, last_copied_line != -1 ? last_copied_line : 0);

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

    for (size_t i = 0; i < ranges.size(); i++)
    {
        assert(!range_empty(ranges[i]));

        const size_t size   = range_size(ranges[i]);
        const size_t offset = dst_buffer.size();

        dst_buffer.resize(offset + size);
        memcpy(dst_buffer.data() + offset, src_buffer.data() + ranges[i].start, size);

        if (ranges[i].end == src_buffer.size())
        {
            dst_buffer[dst_buffer.size() - 1] = '\n';
        }
    }
}

static bool delete_ranges(Array<char>& buffer, const Array<Range>& ranges)
{
    if (!ranges.size())
    {
        return false;
    }

    size_t removed = 0;
    size_t start   = ranges[0].start;
    size_t end     = ranges[0].end;

    for (size_t i = 1; i < ranges.size(); i++)
    {
        while (i < ranges.size() && range_contains({ start, end }, ranges[i].start))
        {
            end = std::max(end, ranges[i].end);
            i++;
        }

        char*        dst  = buffer.data() + start - removed;
        const char*  src  = buffer.data() + end   - removed;
        const size_t size = buffer.size() - end;

        memmove(dst, src, size);
        removed += end - start;

        if (i < ranges.size())
        {
            start = ranges[i].start;
            end   = ranges[i].end;
        }
    }

    buffer.resize(buffer.size() - removed);
    assert(buffer.size());
    assert(buffer[buffer.size() - 1] == 0);

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
        parse_lines(state.buffer.data(), state.lines);
    }

    // Since we're reusing the clipboard range array (to avoid additional heap
    // alloations), we have to adjust the ranges to point into its char buffer.
    for (size_t i = 0, offset = 0; i < clipboard.ranges.size(); i++)
    {
        const size_t size = range_size(clipboard.ranges[i]);

        clipboard.ranges[i] = { offset, offset + size };

        offset += size;
    }
}


// -----------------------------------------------------------------------------
// UNDO/REDO
// -----------------------------------------------------------------------------

// PASTE:
//   - store cursors
//   - store selection contents for undo-pasting
//   - paste the data
//   - store cursors
//
// CUT:
//   - store cursors
//   - store selection contents for undo-pasting
//   - store cursors
//
// CODEPOINT:
//   - if first codepoint in a sequence, do as in "PASTE", except don't store
//     the "after" cursors
//   - for other codepoints, simply append them

// NOTE : The `size_t` indices are quite wasteful on 64-bit system. Consider
//        replacing them with u32, or even smaller types (and perhaps store
//        offsets, rathen than the indices). Could also have some sort of
//        variable size info, but that would make things more complex.
struct HistoryBuffer
{
    Array<uint8_t>& buffer;

    struct Block
    {
        enum Type : uint8_t
        {
            NONE,

            ADD,
            CURSORS,
            REMOVE,
        };

        size_t  prev;
        size_t  next;
        Type    type;
        bool    growable;
        uint8_t payload[2];
    };

    struct Header
    {
        size_t last_block_start = 0;
        size_t read_head        = 0;
    };

    HistoryBuffer(Array<uint8_t>& buffer)
        : buffer(buffer)
    {
        if (buffer.empty())
        {
            buffer.resize(1024 * 1024);
            buffer.clear();

            write(Header());
            (void)start_block(Block::NONE, false);
        }
    }

    inline Header* get_header()
    {
        return reinterpret_cast<Header*>(buffer.data());
    }

    inline Block* get_block_at(size_t offset)
    {
        return reinterpret_cast<Block*>(buffer.data() + offset);
    }

    inline Block* get_last_block()
    {
        return get_block_at(get_header()->last_block_start);
    }

    void undo(State& state)
    {
        Block* block = get_last_block();

        // ...
    }

    void align(size_t alignment)
    {
        assert((alignment & (alignment - 1)) == 0);

        const size_t mask = alignment - 1;
        const size_t size = (buffer.size() + mask) & (~mask);

        buffer.resize(size);
    }

    void write(const void* data, size_t size)
    {
        const size_t offset = buffer.size();

        buffer.resize(offset + size);

        bx::memCopy(&buffer[offset], data, size);
    }

    template <typename T>
    void write(const T& value)
    {
        align(BX_ALIGNOF(T));
        write(&value, sizeof(T));
    }

    void write_cursors(const State& state)
    {
        write<size_t>(state.cursors.size());
        write(state.cursors.data(), state.cursors.size() * sizeof(Cursor));
    }

    void write_selections(const State& state)
    {
        for (size_t i = 0; i < state.cursors.size(); i++)
        {
            if (!range_empty(state.cursors[i].selection))
            {
                write(state.buffer.data() + state.cursors[i].selection.start, range_size(state.cursors[i].selection));
            }
        }
    }

    void write_codepoint(const State& state, const char* codepoint, size_t size)
    {
        const bool is_space = (size == 1 && *codepoint == ' ');
        Block*     block    = get_last_block();

        if (block->type != Block::ADD || !block->growable || (!is_space && block->payload[0]))
        {
            block = start_block(Block::ADD, true);

            write_cursors(state);
            write_selections(state);

            buffer.push_back(0);
        }

        block->payload[0] = is_space;

        buffer.resize(buffer.size() - 1);

        write(codepoint, size);
        buffer.push_back(0);
    }

    Block* start_block(Block::Type type, bool growable)
    {
        Block block    = {};
        block.prev     = get_header()->last_block_start;
        block.type     = type;
        block.growable = growable;

        align(BX_ALIGNOF(Block));

        if (type != Block::NONE)
        {
            get_last_block()->next = buffer.size();
        }

        get_header()->last_block_start = buffer.size();

        write(&block, sizeof(block));

        return get_last_block();
    }
};


// -----------------------------------------------------------------------------
// STATE ACTIONS
// -----------------------------------------------------------------------------

static void action_move_horizontally(State& state, bool left)
{
    for (size_t i = 0; i < state.cursors.size(); i++)
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

        state.cursors[i] = cursor;
    }

    fix_overlapping_cursors(state.cursors);
}

static void action_move_vertically(State& state, bool up)
{
    for (size_t i = 0, start_line = 0; i < state.cursors.size(); i++)
    {
        Cursor cursor = state.cursors[i];
        size_t cursor_line;

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

        state.cursors[i] = cursor;
    }

    fix_overlapping_cursors(state.cursors);
}

static void action_select_horizontally(State& state, bool left)
{
    for (size_t i = 0; i < state.cursors.size(); i++)
    {
        Cursor  cursor = state.cursors[i];
        size_t& stop   = cursor.selection.start == cursor.offset ? cursor.selection.start : cursor.selection.end;

        if (left)
        {
            if (stop > 0)
            {
                stop--;
            }
        }
        else if (stop + 1 < state.buffer.size())
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
    for (size_t i = 0; i < state.cursors.size(); i++)
    {
        Cursor cursor = state.cursors[i];
        size_t line   = to_line(state, cursor.offset); // TODO : Figure out `start_line` logic.

        assert(cursor.offset == cursor.selection.start || cursor.offset == cursor.selection.end);

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
        else if (line + 1 < state.lines.size())
        {
            line++;
        }
        else
        {
            // TODO : Stick to the end of the text.
        }

        const size_t cursor_x = std::min(cursor.preferred_x, line_length(state, line) - 1);
        const size_t offset   = to_offset(state, cursor_x, line);

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

    size_t removed = 0;

    for (size_t i = 0; i < state.cursors.size(); i++)
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
            else if (cursor.selection.end + 1 < state.buffer.size())
            {
                cursor.selection.end++;
            }
        }

        if (!range_empty(cursor.selection))
        {
            char*        dst  = state.buffer.data() + cursor.selection.start - removed;
            const char*  src  = state.buffer.data() + cursor.selection.end   - removed;
            const size_t size = state.buffer.size() - cursor.selection.end;

            memmove(dst, src, size);
            removed += range_size(cursor.selection);

            cursor.selection.start =
            cursor.selection.end   =
            cursor.offset          = dst - state.buffer.data();
            cursor.preferred_x     = to_position(state, cursor.offset).x;
        }
    }

    if (removed)
    {
        state.buffer.resize(state.buffer.size() - removed);
        parse_lines(state.buffer.data(), state.lines);
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
    cursor.offset          = state.lines[state.lines.size() - 1].end - 1;
}

static bool is_word_separator(utf8_int32_t codepoint, const char* ascii_separators)
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
    Cursor& cursor = state.cursors[state.cursors.size() - 1];

    if (!range_empty(cursor.selection))
    {
        return;
    }

    const Range line = state.lines[to_line(state, cursor.offset)];

    if (range_size(line) == 1)
    {
        assert(state.buffer[line.start] == '\n');
        return;
    }

    utf8_int32_t codepoint;
    const char*  start    = state.buffer.data() + cursor.offset;
    const void*  iterator = utf8codepoint(start, &codepoint);
    const bool   category = is_word_separator(codepoint, state.word_separators);

    while (codepoint && codepoint != '\n' && category == is_word_separator(codepoint, state.word_separators))
    {
        cursor.selection.end += utf8codepointsize(codepoint);
        iterator = utf8codepoint(iterator, &codepoint);
    }

    iterator = utf8rcodepoint(start   , &codepoint);
    iterator = utf8rcodepoint(iterator, &codepoint);

    while (codepoint && cursor.selection.start > line.start && category == is_word_separator(codepoint, state.word_separators))
    {
        cursor.selection.start -= utf8codepointsize(codepoint);
        iterator = utf8rcodepoint(iterator, &codepoint);
    }

    cursor.offset = cursor.selection.end;
}

static void action_select_line(State& state)
{
    // NOTE : This should be called right after `click`, so the last cursor in
    //        the array should be the lastly added one.
    Cursor& cursor = state.cursors[state.cursors.size() - 1];

    cursor.selection = state.lines[to_line(state, cursor.offset)];
    cursor.offset    = cursor.selection.end;
}

static void action_tab(State& state)
{
    // TODO : Consider reducing the number of allocations (would require two passes).

    sort_cursors(state.cursors);

    for (size_t i = 0, line = 0, offset = 0; i < state.cursors.size(); i++)
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
            const size_t x = to_column(state, cursor.selection.start, line);
            const size_t n = state.tab_size - (x % state.tab_size);

            paste_string(state, cursor, " ", 1, n);

            offset += n;
        }
        else
        {
            // TODO: Add offset to the beginning of each line intersecting the selection.
        }
    }

    parse_lines(state.buffer.data(), state.lines);
}

static void action_clear_history(State& state)
{
    state.history.clear();
    // HistoryBuffer(state.history).write_cursors(state);
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
    buffer.resize(4096);
    buffer.clear();
    buffer.push_back(0);

    lines.resize(128);
    lines.clear();
    lines.push_back({ 0, 1 });

    cursors.resize(16);
    cursors.clear();
    cursors.push_back({});

    action(Action::CLEAR_HISTORY);

    char_width  = 0.0f;
    line_height = 0.0f;
}

void State::click(float x, float y, bool multi_mode)
{
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

// TODO : Consider adding support for different modes of dragging, similar to
//        VS Code (char / word / line), in conjunction with `Action::SELECT_*`
void State::drag(float x, float y)
{
    // Lastly added cursor is considered the active one to drive the selection.
    const size_t active = cursors.size() - 1;
    size_t       count  = active;

    const Position position = click_position(*this, x, y);
    const size_t   offset   = to_offset(*this, position.x, position.y);

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
            HistoryBuffer(history).undo(*this);
            break;

        case Action::REDO:
            // ...
            break;

        default:
            assert(false && "Not yet implemented.");
    }
}

void State::codepoint(uint32_t codepoint)
{
    char         buffer[4] = { 0 };
    const void*  next      = utf8catcodepoint(buffer, static_cast<utf8_int32_t>(codepoint), 4);
    const size_t size      = static_cast<const char*>(next) - buffer;

    HistoryBuffer(history).write_codepoint(*this, buffer, size);

    paste(buffer, size);
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
    if (clipboard.ranges.size() == 1)
    {
        paste(clipboard.buffer.data() + clipboard.ranges[0].start, range_size(clipboard.ranges[0]));
    }
    else if (clipboard.ranges.size())
    {
        sort_cursors(cursors);

        paste_multi(*this, clipboard);
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

    sort_cursors(cursors);

    for (size_t i = 0, offset = 0; i < cursors.size(); i++)
    {
        cursors[i].selection.start += offset;
        cursors[i].selection.end   += offset;
        cursors[i].offset          += offset;

        offset += paste_string(*this, cursors[i], string, size);
    }

    parse_lines(buffer.data(), lines);

    update_preferred_x(*this);
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
        check_cursor(0, { { 480, 480 }, 480, 25 });
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

static void test_paste_n_n()
{
    TestState state;

    state.clear();
    state.paste(
        ">>A<<\n"
        ">><<\n"
        ">>B<<"
    );

    state.cursors.clear();
    state.cursors.push_back({ {  2,  3 },  3 });
    state.cursors.push_back({ {  8,  8 },  8 });
    state.cursors.push_back({ { 13, 14 }, 13 });

    Clipboard clipboard;
    clipboard.buffer = { '1', '2', '2' ,'3' , '3', '3' };
    clipboard.ranges = { { 0, 1 }, { 1, 3 }, { 3, 6 } };

    state.paste(clipboard);
    state.check_invariants();

    state.check_string(
        ">>1<<\n"
        ">>22<<\n"
        ">>333<<"
    );
}

static void test_paste_m_n()
{
    TestState state;

    state.clear();
    state.paste(
        ">>A<<\n"
        ">><<\n"
        ">>B<<"
    );

    state.cursors.clear();
    state.cursors.push_back({ {  2,  3 },  3 });
    state.cursors.push_back({ { 13, 14 }, 13 });

    Clipboard clipboard;
    clipboard.buffer = { '1', '2', '2' ,'3' , '3', '3' };
    clipboard.ranges = { { 0, 1 }, { 1, 3 }, { 3, 6 } };

    state.paste(clipboard);
    state.check_invariants();

    state.check_string(
        ">>1\n22\n333<<\n"
        ">><<\n"
        ">>1\n22\n333<<"
    );
}

static bool s_tests_done = []()
{
    test_cut();
    test_paste_n_n();
    test_paste_m_n();

    return true;
}();

} // namespace tests

#endif // defined(TED_TESTS) && !defined(NDEBUG)


// -----------------------------------------------------------------------------

} // namespace ted
