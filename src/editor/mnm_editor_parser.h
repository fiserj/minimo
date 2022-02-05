#pragma once

#include <inttypes.h>

#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_c(void);

namespace mnm
{

// The values are taken from `tree-sitter-c/src/parser.c`. It's unlikely that
// they would change, but even so, it'd be great to generate this list automatically.
enum struct AstToken : TSSymbol
{
    SYM_identifier                                       = 1,
    AUX_SYM_preproc_include_token1                       = 2,
    ANON_SYM_LF                                          = 3,
    AUX_SYM_preproc_def_token1                           = 4,
    ANON_SYM_LPAREN                                      = 5,
    ANON_SYM_DOT_DOT_DOT                                 = 6,
    ANON_SYM_COMMA                                       = 7,
    ANON_SYM_RPAREN                                      = 8,
    AUX_SYM_preproc_if_token1                            = 9,
    AUX_SYM_preproc_if_token2                            = 10,
    AUX_SYM_preproc_ifdef_token1                         = 11,
    AUX_SYM_preproc_ifdef_token2                         = 12,
    AUX_SYM_preproc_else_token1                          = 13,
    AUX_SYM_preproc_elif_token1                          = 14,
    SYM_preproc_directive                                = 15,
    SYM_preproc_arg                                      = 16,
    ANON_SYM_LPAREN2                                     = 17,
    ANON_SYM_defined                                     = 18,
    ANON_SYM_BANG                                        = 19,
    ANON_SYM_TILDE                                       = 20,
    ANON_SYM_DASH                                        = 21,
    ANON_SYM_PLUS                                        = 22,
    ANON_SYM_STAR                                        = 23,
    ANON_SYM_SLASH                                       = 24,
    ANON_SYM_PERCENT                                     = 25,
    ANON_SYM_PIPE_PIPE                                   = 26,
    ANON_SYM_AMP_AMP                                     = 27,
    ANON_SYM_PIPE                                        = 28,
    ANON_SYM_CARET                                       = 29,
    ANON_SYM_AMP                                         = 30,
    ANON_SYM_EQ_EQ                                       = 31,
    ANON_SYM_BANG_EQ                                     = 32,
    ANON_SYM_GT                                          = 33,
    ANON_SYM_GT_EQ                                       = 34,
    ANON_SYM_LT_EQ                                       = 35,
    ANON_SYM_LT                                          = 36,
    ANON_SYM_LT_LT                                       = 37,
    ANON_SYM_GT_GT                                       = 38,
    ANON_SYM_SEMI                                        = 39,
    ANON_SYM_typedef                                     = 40,
    ANON_SYM_extern                                      = 41,
    ANON_SYM___attribute__                               = 42,
    ANON_SYM_COLON_COLON                                 = 43,
    ANON_SYM_LBRACK_LBRACK                               = 44,
    ANON_SYM_RBRACK_RBRACK                               = 45,
    ANON_SYM___declspec                                  = 46,
    ANON_SYM___based                                     = 47,
    ANON_SYM___cdecl                                     = 48,
    ANON_SYM___clrcall                                   = 49,
    ANON_SYM___stdcall                                   = 50,
    ANON_SYM___fastcall                                  = 51,
    ANON_SYM___thiscall                                  = 52,
    ANON_SYM___vectorcall                                = 53,
    SYM_ms_restrict_modifier                             = 54,
    SYM_ms_unsigned_ptr_modifier                         = 55,
    SYM_ms_signed_ptr_modifier                           = 56,
    ANON_SYM__unaligned                                  = 57,
    ANON_SYM___unaligned                                 = 58,
    ANON_SYM_LBRACE                                      = 59,
    ANON_SYM_RBRACE                                      = 60,
    ANON_SYM_LBRACK                                      = 61,
    ANON_SYM_RBRACK                                      = 62,
    ANON_SYM_EQ                                          = 63,
    ANON_SYM_static                                      = 64,
    ANON_SYM_auto                                        = 65,
    ANON_SYM_register                                    = 66,
    ANON_SYM_inline                                      = 67,
    ANON_SYM_const                                       = 68,
    ANON_SYM_volatile                                    = 69,
    ANON_SYM_restrict                                    = 70,
    ANON_SYM__Atomic                                     = 71,
    ANON_SYM_signed                                      = 72,
    ANON_SYM_unsigned                                    = 73,
    ANON_SYM_long                                        = 74,
    ANON_SYM_short                                       = 75,
    SYM_primitive_type                                   = 76,
    ANON_SYM_enum                                        = 77,
    ANON_SYM_struct                                      = 78,
    ANON_SYM_union                                       = 79,
    ANON_SYM_COLON                                       = 80,
    ANON_SYM_if                                          = 81,
    ANON_SYM_else                                        = 82,
    ANON_SYM_switch                                      = 83,
    ANON_SYM_case                                        = 84,
    ANON_SYM_default                                     = 85,
    ANON_SYM_while                                       = 86,
    ANON_SYM_do                                          = 87,
    ANON_SYM_for                                         = 88,
    ANON_SYM_return                                      = 89,
    ANON_SYM_break                                       = 90,
    ANON_SYM_continue                                    = 91,
    ANON_SYM_goto                                        = 92,
    ANON_SYM_QMARK                                       = 93,
    ANON_SYM_STAR_EQ                                     = 94,
    ANON_SYM_SLASH_EQ                                    = 95,
    ANON_SYM_PERCENT_EQ                                  = 96,
    ANON_SYM_PLUS_EQ                                     = 97,
    ANON_SYM_DASH_EQ                                     = 98,
    ANON_SYM_LT_LT_EQ                                    = 99,
    ANON_SYM_GT_GT_EQ                                    = 100,
    ANON_SYM_AMP_EQ                                      = 101,
    ANON_SYM_CARET_EQ                                    = 102,
    ANON_SYM_PIPE_EQ                                     = 103,
    ANON_SYM_DASH_DASH                                   = 104,
    ANON_SYM_PLUS_PLUS                                   = 105,
    ANON_SYM_sizeof                                      = 106,
    ANON_SYM_DOT                                         = 107,
    ANON_SYM_DASH_GT                                     = 108,
    SYM_number_literal                                   = 109,
    ANON_SYM_L_SQUOTE                                    = 110,
    ANON_SYM_u_SQUOTE                                    = 111,
    ANON_SYM_U_SQUOTE                                    = 112,
    ANON_SYM_u8_SQUOTE                                   = 113,
    ANON_SYM_SQUOTE                                      = 114,
    AUX_SYM_char_literal_token1                          = 115,
    ANON_SYM_L_DQUOTE                                    = 116,
    ANON_SYM_u_DQUOTE                                    = 117,
    ANON_SYM_U_DQUOTE                                    = 118,
    ANON_SYM_u8_DQUOTE                                   = 119,
    ANON_SYM_DQUOTE                                      = 120,
    AUX_SYM_string_literal_token1                        = 121,
    SYM_escape_sequence                                  = 122,
    SYM_system_lib_string                                = 123,
    SYM_true                                             = 124,
    SYM_false                                            = 125,
    SYM_null                                             = 126,
    SYM_comment                                          = 127,
    SYM_translation_unit                                 = 128,
    SYM_preproc_include                                  = 129,
    SYM_preproc_def                                      = 130,
    SYM_preproc_function_def                             = 131,
    SYM_preproc_params                                   = 132,
    SYM_preproc_call                                     = 133,
    SYM_preproc_if                                       = 134,
    SYM_preproc_ifdef                                    = 135,
    SYM_preproc_else                                     = 136,
    SYM_preproc_elif                                     = 137,
    SYM_preproc_if_in_field_declaration_list             = 138,
    SYM_preproc_ifdef_in_field_declaration_list          = 139,
    SYM_preproc_else_in_field_declaration_list           = 140,
    SYM_preproc_elif_in_field_declaration_list           = 141,
    SYM__preproc_expression                              = 142,
    SYM_preproc_parenthesized_expression                 = 143,
    SYM_preproc_defined                                  = 144,
    SYM_preproc_unary_expression                         = 145,
    SYM_preproc_call_expression                          = 146,
    SYM_preproc_argument_list                            = 147,
    SYM_preproc_binary_expression                        = 148,
    SYM_function_definition                              = 149,
    SYM_declaration                                      = 150,
    SYM_type_definition                                  = 151,
    SYM__declaration_modifiers                           = 152,
    SYM__declaration_specifiers                          = 153,
    SYM_linkage_specification                            = 154,
    SYM_attribute_specifier                              = 155,
    SYM_attribute                                        = 156,
    SYM_attribute_declaration                            = 157,
    SYM_ms_declspec_modifier                             = 158,
    SYM_ms_based_modifier                                = 159,
    SYM_ms_call_modifier                                 = 160,
    SYM_ms_unaligned_ptr_modifier                        = 161,
    SYM_ms_pointer_modifier                              = 162,
    SYM_declaration_list                                 = 163,
    SYM__declarator                                      = 164,
    SYM__field_declarator                                = 165,
    SYM__type_declarator                                 = 166,
    SYM__abstract_declarator                             = 167,
    SYM_parenthesized_declarator                         = 168,
    SYM_parenthesized_field_declarator                   = 169,
    SYM_parenthesized_type_declarator                    = 170,
    SYM_abstract_parenthesized_declarator                = 171,
    SYM_attributed_declarator                            = 172,
    SYM_attributed_field_declarator                      = 173,
    SYM_attributed_type_declarator                       = 174,
    SYM_pointer_declarator                               = 175,
    SYM_pointer_field_declarator                         = 176,
    SYM_pointer_type_declarator                          = 177,
    SYM_abstract_pointer_declarator                      = 178,
    SYM_function_declarator                              = 179,
    SYM_function_field_declarator                        = 180,
    SYM_function_type_declarator                         = 181,
    SYM_abstract_function_declarator                     = 182,
    SYM_array_declarator                                 = 183,
    SYM_array_field_declarator                           = 184,
    SYM_array_type_declarator                            = 185,
    SYM_abstract_array_declarator                        = 186,
    SYM_init_declarator                                  = 187,
    SYM_compound_statement                               = 188,
    SYM_storage_class_specifier                          = 189,
    SYM_type_qualifier                                   = 190,
    SYM__type_specifier                                  = 191,
    SYM_sized_type_specifier                             = 192,
    SYM_enum_specifier                                   = 193,
    SYM_enumerator_list                                  = 194,
    SYM_struct_specifier                                 = 195,
    SYM_union_specifier                                  = 196,
    SYM_field_declaration_list                           = 197,
    SYM__field_declaration_list_item                     = 198,
    SYM_field_declaration                                = 199,
    SYM_bitfield_clause                                  = 200,
    SYM_enumerator                                       = 201,
    SYM_variadic_parameter                               = 202,
    SYM_parameter_list                                   = 203,
    SYM_parameter_declaration                            = 204,
    SYM_attributed_statement                             = 205,
    SYM_attributed_non_case_statement                    = 206,
    SYM_labeled_statement                                = 207,
    SYM_expression_statement                             = 208,
    SYM_if_statement                                     = 209,
    SYM_switch_statement                                 = 210,
    SYM_case_statement                                   = 211,
    SYM_while_statement                                  = 212,
    SYM_do_statement                                     = 213,
    SYM_for_statement                                    = 214,
    SYM_return_statement                                 = 215,
    SYM_break_statement                                  = 216,
    SYM_continue_statement                               = 217,
    SYM_goto_statement                                   = 218,
    SYM__expression                                      = 219,
    SYM_comma_expression                                 = 220,
    SYM_conditional_expression                           = 221,
    SYM_assignment_expression                            = 222,
    SYM_pointer_expression                               = 223,
    SYM_unary_expression                                 = 224,
    SYM_binary_expression                                = 225,
    SYM_update_expression                                = 226,
    SYM_cast_expression                                  = 227,
    SYM_type_descriptor                                  = 228,
    SYM_sizeof_expression                                = 229,
    SYM_subscript_expression                             = 230,
    SYM_call_expression                                  = 231,
    SYM_argument_list                                    = 232,
    SYM_field_expression                                 = 233,
    SYM_compound_literal_expression                      = 234,
    SYM_parenthesized_expression                         = 235,
    SYM_initializer_list                                 = 236,
    SYM_initializer_pair                                 = 237,
    SYM_subscript_designator                             = 238,
    SYM_field_designator                                 = 239,
    SYM_char_literal                                     = 240,
    SYM_concatenated_string                              = 241,
    SYM_string_literal                                   = 242,
    SYM__empty_declaration                               = 243,
    SYM_macro_type_specifier                             = 244,
    AUX_SYM_translation_unit_repeat1                     = 245,
    AUX_SYM_preproc_params_repeat1                       = 246,
    AUX_SYM_preproc_if_in_field_declaration_list_repeat1 = 247,
    AUX_SYM_preproc_argument_list_repeat1                = 248,
    AUX_SYM_declaration_repeat1                          = 249,
    AUX_SYM_type_definition_repeat1                      = 250,
    AUX_SYM_type_definition_repeat2                      = 251,
    AUX_SYM__declaration_specifiers_repeat1              = 252,
    AUX_SYM_attribute_declaration_repeat1                = 253,
    AUX_SYM_attributed_declarator_repeat1                = 254,
    AUX_SYM_pointer_declarator_repeat1                   = 255,
    AUX_SYM_function_declarator_repeat1                  = 256,
    AUX_SYM_sized_type_specifier_repeat1                 = 257,
    AUX_SYM_enumerator_list_repeat1                      = 258,
    AUX_SYM_field_declaration_repeat1                    = 259,
    AUX_SYM_parameter_list_repeat1                       = 260,
    AUX_SYM_case_statement_repeat1                       = 261,
    AUX_SYM_argument_list_repeat1                        = 262,
    AUX_SYM_initializer_list_repeat1                     = 263,
    AUX_SYM_initializer_pair_repeat1                     = 264,
    AUX_SYM_concatenated_string_repeat1                  = 265,
    AUX_SYM_string_literal_repeat1                       = 266,
    ALIAS_SYM_field_identifier                           = 267,
    ALIAS_SYM_statement_identifier                       = 268,
    ALIAS_SYM_type_identifier                            = 269,
};

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
