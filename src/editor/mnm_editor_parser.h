#pragma once

#include <inttypes.h>

#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_c(void);

namespace mnm
{

// NOTE : Consult `tree-sitter-c/src/parser.c` for the symbol values
static bool is_symbol_printable(TSSymbol symbol)
{
    switch (symbol)
    {
    case   1: // sym_identifier
    case   2: // aux_sym_preproc_include_token1
    case   4: // aux_sym_preproc_def_token1
    case   5: // anon_sym_LPAREN
    case   6: // anon_sym_DOT_DOT_DOT
    case   7: // anon_sym_COMMA
    case   8: // anon_sym_RPAREN
    case   9: // aux_sym_preproc_if_token1
    case  10: // aux_sym_preproc_if_token2
    case  11: // aux_sym_preproc_ifdef_token1
    case  12: // aux_sym_preproc_ifdef_token2
    case  13: // aux_sym_preproc_else_token1
    case  14: // aux_sym_preproc_elif_token1
    case  15: // sym_preproc_directive
    case  16: // sym_preproc_arg
    case  17: // anon_sym_LPAREN2
    case  18: // anon_sym_defined
    case  19: // anon_sym_BANG
    case  20: // anon_sym_TILDE
    case  21: // anon_sym_DASH
    case  22: // anon_sym_PLUS
    case  23: // anon_sym_STAR
    case  24: // anon_sym_SLASH
    case  25: // anon_sym_PERCENT
    case  26: // anon_sym_PIPE_PIPE
    case  27: // anon_sym_AMP_AMP
    case  28: // anon_sym_PIPE
    case  29: // anon_sym_CARET
    case  30: // anon_sym_AMP
    case  31: // anon_sym_EQ_EQ
    case  32: // anon_sym_BANG_EQ
    case  33: // anon_sym_GT
    case  34: // anon_sym_GT_EQ
    case  35: // anon_sym_LT_EQ
    case  36: // anon_sym_LT
    case  37: // anon_sym_LT_LT
    case  38: // anon_sym_GT_GT
    case  39: // anon_sym_SEMI
    case  40: // anon_sym_typedef
    case  41: // anon_sym_extern
    case  59: // anon_sym_LBRACE
    case  60: // anon_sym_RBRACE
    case  61: // anon_sym_LBRACK
    case  62: // anon_sym_RBRACK
    case  63: // anon_sym_EQ
    case  64: // anon_sym_static
    case  65: // anon_sym_auto
    case  66: // anon_sym_register
    case  67: // anon_sym_inline
    case  68: // anon_sym_const
    case  69: // anon_sym_volatile
    case  70: // anon_sym_restrict
    case  71: // anon_sym__Atomic
    case  72: // anon_sym_signed
    case  73: // anon_sym_unsigned
    case  74: // anon_sym_long
    case  75: // anon_sym_short
    case  76: // sym_primitive_type
    case  77: // anon_sym_enum
    case  78: // anon_sym_struct
    case  79: // anon_sym_union
    case  80: // anon_sym_COLON
    case  81: // anon_sym_if
    case  82: // anon_sym_else
    case  83: // anon_sym_switch
    case  84: // anon_sym_case
    case  85: // anon_sym_default
    case  86: // anon_sym_while
    case  87: // anon_sym_do
    case  88: // anon_sym_for
    case  89: // anon_sym_return
    case  90: // anon_sym_break
    case  91: // anon_sym_continue
    case  92: // anon_sym_goto
    case  93: // anon_sym_QMARK
    case  94: // anon_sym_STAR_EQ
    case  95: // anon_sym_SLASH_EQ
    case  96: // anon_sym_PERCENT_EQ
    case  97: // anon_sym_PLUS_EQ
    case  98: // anon_sym_DASH_EQ
    case  99: // anon_sym_LT_LT_EQ
    case 100: // anon_sym_GT_GT_EQ
    case 101: // anon_sym_AMP_EQ
    case 102: // anon_sym_CARET_EQ
    case 103: // anon_sym_PIPE_EQ
    case 104: // anon_sym_DASH_DASH
    case 105: // anon_sym_PLUS_PLUS
    case 106: // anon_sym_sizeof
    case 107: // anon_sym_DOT
    case 108: // anon_sym_DASH_GT
    case 109: // sym_number_literal
    case 123: // sym_system_lib_string
    case 124: // sym_true
    case 125: // sym_false
    case 126: // sym_null
    case 127: // sym_comment
    case 242: // sym_string_literal
        return true;

    case   3: // anon_sym_LF
    case 129: // sym_preproc_include
    case 130: // sym_preproc_def
    case 150: // sym_declaration
    case 179: // sym_function_declarator 
    case 189: // sym_storage_class_specifier
    case 203: // sym_parameter_list
        return false;

    default:
        ASSERT(false && "WIP : Map out the tokens that should not be printed");
        return false;
    };
}

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

        // NOTE : If symbol is printable, we stop descending.
        if (is_symbol_printable(ts_node_symbol(node)))
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
        else if (ts_tree_cursor_goto_first_child (&cursor))
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
