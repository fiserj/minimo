#ifndef TED_H
#define TED_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TED_STATIC
#   define TED_API static
#else
#   define TED_API extern
#endif

/// Actions.
///
typedef enum ted_Action
{
    TED_ACTION_MOVE_LEFT,
    TED_ACTION_MOVE_RIGHT,
    TED_ACTION_MOVE_UP,
    TED_ACTION_MOVE_DOWN,

    TED_ACTION_GO_BACK,
    TED_ACTION_GO_FORWARD,

    TED_ACTION_MOVE_LINE_UP,
    TED_ACTION_MOVE_LINE_DOWN,

    TED_ACTION_COUNT,

} ted_Action;

/// Range of bytes in a contiguous memory block.
///
typedef struct ted_Range
{
    int start;
    int end; // "One past".

} ted_Range;

/// Cursor with possible selection.
///
typedef struct ted_Cursor
{
    ted_Range selection;
    int       offset;
    int       preferred_x;

} ted_Cursor;

/// Document state. Initialize with `ted_init`, then setup the parameters.
///
typedef struct ted_State
{
    char*       buffer;
    int         buffer_size;
    int         max_buffer_size;

    ted_Range*  lines;
    int         line_count;
    int         max_line_count;

    ted_Cursor* cursors; // Cursors are guaranteed to not overlap.
    int         cursor_count;
    int         max_cursors;

    float       char_width;
    float       line_height;

} ted_State;

TED_API void ted_init(ted_State* state);

TED_API void ted_click(ted_State* state, float x, float y, int multi_mode);

TED_API void ted_drag(ted_State* state, float x, float y, int multi_mode);

TED_API void ted_action(ted_State* state, ted_Action action);

TED_API void ted_codepoint(ted_State* state, int codepoint);

TED_API void ted_cut(ted_State* state);

TED_API void ted_paste(ted_State* state, const char* string, int length);

#ifdef __cplusplus
}
#endif

#endif // TED_H


#ifdef TED_IMPLEMENTATION

#ifndef TED__ASSERT
#   include <assert.h>
#   define TED__ASSERT(cond) assert(cond)
#endif

enum
{
    TED__UTF8_ACCEPT = 0,
};

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>.
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
static const unsigned char ted__utf8_data[] =
{
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
     0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12, 
};

static int inline ted__utf8_decode(int* state, int* codepoint, int byte)
{
    const int type = ted__utf8_data[byte];

    *codepoint = (*state != TED__UTF8_ACCEPT)
        ? (byte & 0x3fu) | (*codepoint << 6)
        : (0xff >> type) & (byte);

    *state = ted__utf8_data[256 + *state + type];

    return *state;
}

static const char* ted__utf8_advance_max_characters(const char* string, int max_characters)
{
    int state     = TED__UTF8_ACCEPT;
    int codepoint = 0;

    for (; *string && max_characters; string++)
    {
        if (TED__UTF8_ACCEPT == ted__utf8_decode(&state, &codepoint, *string))
        {
            max_characters--;
        }
    }

    TED__ASSERT(state == TED__UTF8_ACCEPT);

    return string;
}

static const char* ted__utf8_advance_max_bytes(const char* string, int max_bytes)
{
    int state     = TED__UTF8_ACCEPT;
    int codepoint = 0;

    for (const char* start = string; *string && string - start < max_bytes; string++)
    {
        (void)ted__utf8_decode(&state, &codepoint, *string);
    }

    TED__ASSERT(state == TED__UTF8_ACCEPT);

    return string;
}

static int ted__utf8_length_max_bytes(const char* string, int max_bytes)
{
    TED__ASSERT(string);
    TED__ASSERT(max_bytes >= 0);

    const char* end = string + max_bytes;

    int state     = TED__UTF8_ACCEPT;
    int codepoint = 0;
    int length    = 0;

    for (; *string && string != end; string++)
    {
        if (TED__UTF8_ACCEPT == ted__utf8_decode(&state, &codepoint, *string))
        {
            length++;
        }
    }

    TED__ASSERT(state == TED__UTF8_ACCEPT);

    return length;
}

// static inline char* ted__utf8_next(char* string, int* codepoint)
// {
//     int state = 0;

//     for (; *string && ted__utf8_decode(&state, codepoint, *string); string++)
//     {
//     }

//     TED__ASSERT(state == TED__UTF8_ACCEPT);

//     return string;
// }

static inline int ted__imin(int x, int y)
{
    return x < y ? x : y;
}

static inline int ted__imax(int x, int y)
{
    return x > y ? x : y;
}

static inline float ted__fmin(float x, float y)
{
    return x < y ? x : y;
}

static inline float ted__fmax(float x, float y)
{
    return x > y ? x : y;
}

static inline int ted__roundi(float x)
{
    TED__ASSERT(x >= 0.0f);

    return (int)(x + 0.5f);
}

static inline int ted__range_empty(ted_Range range)
{
    return range.start == range.end;
}

static inline int ted__range_contains(ted_Range range, int offset)
{
    return range.start <= offset && range.end + ted__range_empty(range) > offset;
}

static inline int ted__range_overlap(ted_Range first, ted_Range second)
{
    return
        (first.start >= second.start && first.end   <= second.end) ||
        (first.start >= second.start && first.start <= second.end) ||
        (first.end   >= second.start && first.end   <= second.end) ;
}

static ted_Range ted__range_intersection(ted_Range first, ted_Range second)
{
    ted_Range range;
    range.start = ted__fmax(first.start, second.start);
    range.end   = ted__fmin(first.end  , second.end  );

    if (range.start > range.end)
    {
        range.start = 0.0f;
        range.end   = 0.0f;
    }

    return range;
}

static inline const char* ted__line_start(ted_State* state, int line)
{
    TED__ASSERT(state);
    TED__ASSERT(line >= 0 && line < state->line_count);

    return state->buffer + state->lines[line].start;
}

// static inline const char* ted__line_end(ted_State* state, int line)
// {
//     TED__ASSERT(state);
//     TED__ASSERT(line >= 0 && line < state->line_count);

//     return state->buffer + state->lines[line].end;
// }

static inline int ted__line_bytes(ted_State* state, int line)
{
    TED__ASSERT(state);
    TED__ASSERT(line >= 0 && line < state->line_count);

    return state->lines[line].end - state->lines[line].start;
}

static int ted__line_length(ted_State* state, int line)
{
    TED__ASSERT(state);
    TED__ASSERT(line >= 0 && line < state->line_count);

    return ted__utf8_length_max_bytes(
        ted__line_start(state, line),
        ted__line_bytes(state, line)
    );
}

static inline int ted__offset(ted_State* state, int x, int y)
{
    TED__ASSERT(state);
    TED__ASSERT(x >= 0);
    TED__ASSERT(y >= 0 && y < state->line_count);

    const char* string = ted__utf8_advance_max_characters(state->buffer + state->lines[y].start, x);

    return string - state->buffer;
}

static void ted__position(ted_State* state, int offset, int* x, int* y)
{
    TED__ASSERT(state);
    TED__ASSERT(offset >= 0 && offset < state->lines[state->line_count - 1].end);
    TED__ASSERT(x || y);

    for (int i = 0; i < state->line_count; i++)
    {
        if (ted__range_contains(state->lines[i], offset))
        {
            if (y)
            {
                *y = i;
            }

            if (x)
            {
                *x = ted__utf8_length_max_bytes(
                    ted__line_start(state, i),
                    offset - state->lines[i].start
                );
            }
        }
    }

    TED__ASSERT(0 && "This should not happen.");
}

static inline void ted__ensure_init(ted_State* state)
{
    TED__ASSERT(state);

    if (!state->buffer_size && state->buffer)
    {
        TED__ASSERT(state->lines);

        state->buffer_size    = 1;
        state->buffer[0]      = 0;

        state->line_count     = 1;
        state->lines[0].start = 0;
        state->lines[0].end   = 1;
    }
}

void ted_init(ted_State* state)
{
    TED__ASSERT(state);

    state->buffer          = 0;
    state->buffer_size     = 0;
    state->max_buffer_size = 0;

    state->lines           = 0;
    state->line_count      = 0;
    state->max_line_count  = 0;

    state->cursors         = 0;
    state->cursor_count    = 0;
    state->max_cursors     = 0;

    state->char_width      = 0.0f;
    state->line_height     = 0.0f;
}

static void ted__remove_cursor(ted_State* state, int cursor)
{
    TED__ASSERT(state);
    TED__ASSERT(state->cursor_count > 1);
    TED__ASSERT(cursor >= 0 && cursor < state->cursor_count);

    for (int i = cursor + 1; i < state->cursor_count; i++)
    {
        state->cursors[i - 1] = state->cursors[i];
    }

    state->cursor_count--;
}

static int ted__remove_cursor_containing_offset(ted_State* state, int offset)
{
    TED__ASSERT(state);
    TED__ASSERT(state->line_count > 0);
    TED__ASSERT(offset >= 0 && offset < state->lines[state->line_count - 1].end);

    for (int i = 0; i < state->cursor_count; i++)
    {
        if (ted__range_contains(state->cursors[i].selection, offset))
        {
            if (state->cursor_count > 1)
            {
                ted__remove_cursor(state, i);
                return 1;
            }
        }
    }

    return 0;
}

void ted_click(ted_State* state, float x, float y, int multi_mode)
{
    TED__ASSERT(state);

    ted__ensure_init(state);

    x = ted__fmax(x, 0.0f);
    y = ted__fmax(y, 0.0f);

    const int yi = ted__imin(state->line_count, (int)(y / state->line_height));
    const int xi = ted__imin(ted__roundi(x / state->char_width), ted__line_length(state, yi) - 1);

    const int   offset = ted__offset(state, xi, yi);
    ted_Cursor* cursor = 0;

    if (multi_mode)
    {
        if (!ted__remove_cursor_containing_offset(state, offset) &&
            state->cursor_count < state->max_cursors)
        {
            cursor = state->cursors + state->cursor_count++;
        }
    }
    else
    {
        cursor = state->cursors;
        state->cursor_count = 1;
    }

    if (cursor)
    {
        cursor->selection.start =
        cursor->selection.end   =
        cursor->offset          = offset;
        cursor->preferred_x     = xi;
    }
}

static void ted__fix_overlapping_cursors(ted_State* state)
{
    TED__ASSERT(state);

    if (state->cursor_count < 2)
    {
        return;
    }

    // TODO
}

static void ted__action_move_horizontally(ted_State* state, int diff)
{
    TED__ASSERT(state);
    TED__ASSERT(diff == -1 || diff == 1);

    for (int i = 0; i < state->cursor_count; i++)
    {
        ted_Cursor* cursor = state->cursors + i;

        if (ted__range_empty(cursor->selection))
        {
        
            cursor->selection.start =
            cursor->selection.end   =
            cursor->offset          = 0; // TODO : +/- current/prev codepoint size.
        }
        else
        {
            cursor->selection.start =
            cursor->selection.end   =
            cursor->offset          = diff < 0
                ? cursor->selection.start
                : cursor->selection.end;
        }

        ted__position(state, cursor->offset, &cursor->preferred_x, 0);
    }

    ted__fix_overlapping_cursors(state);
}

void ted_action(ted_State* state, ted_Action action)
{
    TED__ASSERT(state);
    TED__ASSERT(action >= 0 && action < TED_ACTION_COUNT);

    ted__ensure_init(state);

    switch (action)
    {
    case TED_ACTION_MOVE_LEFT:
        ted__action_move_horizontally(state, -1);
        break;

    case TED_ACTION_MOVE_RIGHT:
        ted__action_move_horizontally(state, +1);
        break;

    default:
        TED__ASSERT(0 && "Unhandeled action.");
    }
}

#endif // TED_IMPLEMENTATION
