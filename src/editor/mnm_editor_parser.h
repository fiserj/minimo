#pragma once

#include <inttypes.h>
#include <stdint.h>

#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_c(void);

namespace mnm
{

// TODO : Compress this to use 1 bit per record.
// See `tree_sitter_helper.py` script in `tools` folder.
static const uint8_t s_symbol_printable[] =
{
    1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1,
    0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static void lay_syntax_highlighted_text
(
    gui::Context&     ctx,
    float             x,
    float             y,
    const tes::State& text,
    TSTreeCursor&     cursor,
    uint32_t          start_line,
    uint32_t          end_line,
    uint32_t          max_chars
)
{
    if (-1 == ts_tree_cursor_goto_first_child_for_byte(&cursor, text.lines[start_line].start))
    {
        return;
    }

    for (;;)
    {
        const TSNode node = ts_tree_cursor_current_node(&cursor);

        if (ts_node_start_byte(node) >= text.lines[end_line].end)
        {
            break;
        }

        if (s_symbol_printable[ts_node_symbol(node)])
        {
            const TSPoint point = ts_node_start_point(node);
            // ASSERT(point.column <= max_chars);

            ctx.text(
                text.buffer.data + ts_node_start_byte(node),
                text.buffer.data + ts_node_end_byte(node),
                max_chars,// - point.column,
                gui::COLOR_EDITOR_TEXT,
                x + point.column * text.char_width,
                y + (point.row - start_line) * text.line_height
            );
        }
        else if (ts_tree_cursor_goto_first_child(&cursor))
        {
            continue;
        }

        if (ts_tree_cursor_goto_next_sibling(&cursor))
        {
            continue;
        }

        retrace:

        if (!ts_tree_cursor_goto_parent(&cursor))
        {
            break;
        }

        if (!ts_tree_cursor_goto_next_sibling(&cursor))
        {
            goto retrace;
        }
    }
}

static void dump_tree_sitter_node
(
    const char* source_code,
    uint32_t    range_start,
    uint32_t    range_end,
    TSNode      node,
    int         indent = 0
)
{
    const uint32_t start  = ts_node_start_byte(node);
    const uint32_t end    = ts_node_end_byte  (node);

    if (120 == ts_node_symbol(node))
    {
        int asd = 123;
    }

    if ((start > range_end) | (end < range_start))
    {
        return;
    }

    const uint32_t n = ts_node_child_count(node);

    if (n == 0)
    {
        const uint16_t symbol = ts_node_symbol    (node);

        const char*    str = source_code + start;
        const uint32_t len = end - start;

        printf("[%4" PRIu16 "] %*s%.*s%s", symbol, indent, "", len, str, *str != '\n' ? "\n" : "");
    }

    for (uint32_t i = 0; i < n; i++)
    {
        dump_tree_sitter_node(source_code, range_start, range_end, ts_node_child(node, i), indent + 2);
    }
}

const char* dump_node
(
    TSNode node,
    int    depth,
    char*  buffer,
    int&   offset,
    int&   capacity
)
{
    const uint32_t start_byte = ts_node_start_byte(node);
    const uint32_t end_byte   = ts_node_end_byte  (node);

    const int m = snprintf(
        buffer + offset,
        size_t(capacity),
        "%*s{%s:%" PRIu16 "} from (%" PRIu32 ") to (%" PRIu32 ")\n",
        (depth * 2),
        "",
        ts_node_type(node),
        ts_node_symbol(node),
        start_byte,
        end_byte
    );

    if (m < 0 || m >= capacity)
    {
        return nullptr;
    }

    capacity -= m;
    offset   += m;

    for (uint32_t i = 0, n = ts_node_child_count(node); i < n; i++)
    {
        if (!dump_node(
            ts_node_child(node, i),
            depth + 1,
            buffer,
            offset,
            capacity
        ))
        {
            return nullptr;
        }
    }

    return buffer;
};

static const char* dump_ast(TSNode node)
{
    char buffer[1 << 20];
    int  offset   = 0;
    int  capacity = sizeof(buffer);

    return dump_node(node, 0, buffer, offset, capacity);
}

static void test_tree_sitter(const char* source_code)
{
    TSParser* parser = ts_parser_new();

    ts_parser_set_language(parser, tree_sitter_c());

    TSTree *tree = ts_parser_parse_string(
        parser,
        nullptr,
        source_code,
        strlen(source_code)
    );

    if (const char* dump = dump_ast(ts_tree_root_node(tree)))
    {
        if (FILE* f = fopen("./TEST.log", "wb"))
        {
            fputs(dump, f);
            fclose(f);
        }
    }

    // dump_tree_sitter_node(source_code, 0, 325, ts_tree_root_node(tree));

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

} // namespace mnm
