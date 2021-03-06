import json

from pathlib import Path

file_path = Path(__file__).absolute().parent.parent.parent \
    .joinpath('third_party', 'tree-sitter-c', 'src', 'node-types.json')

with open(file_path) as json_file:
    data = json.load(json_file)

c_enums = {
    "sym_identifier" : 1,
    "aux_sym_preproc_include_token1" : 2,
    "anon_sym_LF" : 3,
    "aux_sym_preproc_def_token1" : 4,
    "anon_sym_LPAREN" : 5,
    "anon_sym_DOT_DOT_DOT" : 6,
    "anon_sym_COMMA" : 7,
    "anon_sym_RPAREN" : 8,
    "aux_sym_preproc_if_token1" : 9,
    "aux_sym_preproc_if_token2" : 10,
    "aux_sym_preproc_ifdef_token1" : 11,
    "aux_sym_preproc_ifdef_token2" : 12,
    "aux_sym_preproc_else_token1" : 13,
    "aux_sym_preproc_elif_token1" : 14,
    "sym_preproc_directive" : 15,
    "sym_preproc_arg" : 16,
    "anon_sym_LPAREN2" : 17,
    "anon_sym_defined" : 18,
    "anon_sym_BANG" : 19,
    "anon_sym_TILDE" : 20,
    "anon_sym_DASH" : 21,
    "anon_sym_PLUS" : 22,
    "anon_sym_STAR" : 23,
    "anon_sym_SLASH" : 24,
    "anon_sym_PERCENT" : 25,
    "anon_sym_PIPE_PIPE" : 26,
    "anon_sym_AMP_AMP" : 27,
    "anon_sym_PIPE" : 28,
    "anon_sym_CARET" : 29,
    "anon_sym_AMP" : 30,
    "anon_sym_EQ_EQ" : 31,
    "anon_sym_BANG_EQ" : 32,
    "anon_sym_GT" : 33,
    "anon_sym_GT_EQ" : 34,
    "anon_sym_LT_EQ" : 35,
    "anon_sym_LT" : 36,
    "anon_sym_LT_LT" : 37,
    "anon_sym_GT_GT" : 38,
    "anon_sym_SEMI" : 39,
    "anon_sym_typedef" : 40,
    "anon_sym_extern" : 41,
    "anon_sym___attribute__" : 42,
    "anon_sym_COLON_COLON" : 43,
    "anon_sym_LBRACK_LBRACK" : 44,
    "anon_sym_RBRACK_RBRACK" : 45,
    "anon_sym___declspec" : 46,
    "anon_sym___based" : 47,
    "anon_sym___cdecl" : 48,
    "anon_sym___clrcall" : 49,
    "anon_sym___stdcall" : 50,
    "anon_sym___fastcall" : 51,
    "anon_sym___thiscall" : 52,
    "anon_sym___vectorcall" : 53,
    "sym_ms_restrict_modifier" : 54,
    "sym_ms_unsigned_ptr_modifier" : 55,
    "sym_ms_signed_ptr_modifier" : 56,
    "anon_sym__unaligned" : 57,
    "anon_sym___unaligned" : 58,
    "anon_sym_LBRACE" : 59,
    "anon_sym_RBRACE" : 60,
    "anon_sym_LBRACK" : 61,
    "anon_sym_RBRACK" : 62,
    "anon_sym_EQ" : 63,
    "anon_sym_static" : 64,
    "anon_sym_auto" : 65,
    "anon_sym_register" : 66,
    "anon_sym_inline" : 67,
    "anon_sym_const" : 68,
    "anon_sym_volatile" : 69,
    "anon_sym_restrict" : 70,
    "anon_sym__Atomic" : 71,
    "anon_sym_signed" : 72,
    "anon_sym_unsigned" : 73,
    "anon_sym_long" : 74,
    "anon_sym_short" : 75,
    "sym_primitive_type" : 76,
    "anon_sym_enum" : 77,
    "anon_sym_struct" : 78,
    "anon_sym_union" : 79,
    "anon_sym_COLON" : 80,
    "anon_sym_if" : 81,
    "anon_sym_else" : 82,
    "anon_sym_switch" : 83,
    "anon_sym_case" : 84,
    "anon_sym_default" : 85,
    "anon_sym_while" : 86,
    "anon_sym_do" : 87,
    "anon_sym_for" : 88,
    "anon_sym_return" : 89,
    "anon_sym_break" : 90,
    "anon_sym_continue" : 91,
    "anon_sym_goto" : 92,
    "anon_sym_QMARK" : 93,
    "anon_sym_STAR_EQ" : 94,
    "anon_sym_SLASH_EQ" : 95,
    "anon_sym_PERCENT_EQ" : 96,
    "anon_sym_PLUS_EQ" : 97,
    "anon_sym_DASH_EQ" : 98,
    "anon_sym_LT_LT_EQ" : 99,
    "anon_sym_GT_GT_EQ" : 100,
    "anon_sym_AMP_EQ" : 101,
    "anon_sym_CARET_EQ" : 102,
    "anon_sym_PIPE_EQ" : 103,
    "anon_sym_DASH_DASH" : 104,
    "anon_sym_PLUS_PLUS" : 105,
    "anon_sym_sizeof" : 106,
    "anon_sym_DOT" : 107,
    "anon_sym_DASH_GT" : 108,
    "sym_number_literal" : 109,
    "anon_sym_L_SQUOTE" : 110,
    "anon_sym_u_SQUOTE" : 111,
    "anon_sym_U_SQUOTE" : 112,
    "anon_sym_u8_SQUOTE" : 113,
    "anon_sym_SQUOTE" : 114,
    "aux_sym_char_literal_token1" : 115,
    "anon_sym_L_DQUOTE" : 116,
    "anon_sym_u_DQUOTE" : 117,
    "anon_sym_U_DQUOTE" : 118,
    "anon_sym_u8_DQUOTE" : 119,
    "anon_sym_DQUOTE" : 120,
    "aux_sym_string_literal_token1" : 121,
    "sym_escape_sequence" : 122,
    "sym_system_lib_string" : 123,
    "sym_true" : 124,
    "sym_false" : 125,
    "sym_null" : 126,
    "sym_comment" : 127,
    "sym_translation_unit" : 128,
    "sym_preproc_include" : 129,
    "sym_preproc_def" : 130,
    "sym_preproc_function_def" : 131,
    "sym_preproc_params" : 132,
    "sym_preproc_call" : 133,
    "sym_preproc_if" : 134,
    "sym_preproc_ifdef" : 135,
    "sym_preproc_else" : 136,
    "sym_preproc_elif" : 137,
    "sym_preproc_if_in_field_declaration_list" : 138,
    "sym_preproc_ifdef_in_field_declaration_list" : 139,
    "sym_preproc_else_in_field_declaration_list" : 140,
    "sym_preproc_elif_in_field_declaration_list" : 141,
    "sym__preproc_expression" : 142,
    "sym_preproc_parenthesized_expression" : 143,
    "sym_preproc_defined" : 144,
    "sym_preproc_unary_expression" : 145,
    "sym_preproc_call_expression" : 146,
    "sym_preproc_argument_list" : 147,
    "sym_preproc_binary_expression" : 148,
    "sym_function_definition" : 149,
    "sym_declaration" : 150,
    "sym_type_definition" : 151,
    "sym__declaration_modifiers" : 152,
    "sym__declaration_specifiers" : 153,
    "sym_linkage_specification" : 154,
    "sym_attribute_specifier" : 155,
    "sym_attribute" : 156,
    "sym_attribute_declaration" : 157,
    "sym_ms_declspec_modifier" : 158,
    "sym_ms_based_modifier" : 159,
    "sym_ms_call_modifier" : 160,
    "sym_ms_unaligned_ptr_modifier" : 161,
    "sym_ms_pointer_modifier" : 162,
    "sym_declaration_list" : 163,
    "sym__declarator" : 164,
    "sym__field_declarator" : 165,
    "sym__type_declarator" : 166,
    "sym__abstract_declarator" : 167,
    "sym_parenthesized_declarator" : 168,
    "sym_parenthesized_field_declarator" : 169,
    "sym_parenthesized_type_declarator" : 170,
    "sym_abstract_parenthesized_declarator" : 171,
    "sym_attributed_declarator" : 172,
    "sym_attributed_field_declarator" : 173,
    "sym_attributed_type_declarator" : 174,
    "sym_pointer_declarator" : 175,
    "sym_pointer_field_declarator" : 176,
    "sym_pointer_type_declarator" : 177,
    "sym_abstract_pointer_declarator" : 178,
    "sym_function_declarator" : 179,
    "sym_function_field_declarator" : 180,
    "sym_function_type_declarator" : 181,
    "sym_abstract_function_declarator" : 182,
    "sym_array_declarator" : 183,
    "sym_array_field_declarator" : 184,
    "sym_array_type_declarator" : 185,
    "sym_abstract_array_declarator" : 186,
    "sym_init_declarator" : 187,
    "sym_compound_statement" : 188,
    "sym_storage_class_specifier" : 189,
    "sym_type_qualifier" : 190,
    "sym__type_specifier" : 191,
    "sym_sized_type_specifier" : 192,
    "sym_enum_specifier" : 193,
    "sym_enumerator_list" : 194,
    "sym_struct_specifier" : 195,
    "sym_union_specifier" : 196,
    "sym_field_declaration_list" : 197,
    "sym__field_declaration_list_item" : 198,
    "sym_field_declaration" : 199,
    "sym_bitfield_clause" : 200,
    "sym_enumerator" : 201,
    "sym_variadic_parameter" : 202,
    "sym_parameter_list" : 203,
    "sym_parameter_declaration" : 204,
    "sym_attributed_statement" : 205,
    "sym_attributed_non_case_statement" : 206,
    "sym_labeled_statement" : 207,
    "sym_expression_statement" : 208,
    "sym_if_statement" : 209,
    "sym_switch_statement" : 210,
    "sym_case_statement" : 211,
    "sym_while_statement" : 212,
    "sym_do_statement" : 213,
    "sym_for_statement" : 214,
    "sym_return_statement" : 215,
    "sym_break_statement" : 216,
    "sym_continue_statement" : 217,
    "sym_goto_statement" : 218,
    "sym__expression" : 219,
    "sym_comma_expression" : 220,
    "sym_conditional_expression" : 221,
    "sym_assignment_expression" : 222,
    "sym_pointer_expression" : 223,
    "sym_unary_expression" : 224,
    "sym_binary_expression" : 225,
    "sym_update_expression" : 226,
    "sym_cast_expression" : 227,
    "sym_type_descriptor" : 228,
    "sym_sizeof_expression" : 229,
    "sym_subscript_expression" : 230,
    "sym_call_expression" : 231,
    "sym_argument_list" : 232,
    "sym_field_expression" : 233,
    "sym_compound_literal_expression" : 234,
    "sym_parenthesized_expression" : 235,
    "sym_initializer_list" : 236,
    "sym_initializer_pair" : 237,
    "sym_subscript_designator" : 238,
    "sym_field_designator" : 239,
    "sym_char_literal" : 240,
    "sym_concatenated_string" : 241,
    "sym_string_literal" : 242,
    "sym__empty_declaration" : 243,
    "sym_macro_type_specifier" : 244,
    "aux_sym_translation_unit_repeat1" : 245,
    "aux_sym_preproc_params_repeat1" : 246,
    "aux_sym_preproc_if_in_field_declaration_list_repeat1" : 247,
    "aux_sym_preproc_argument_list_repeat1" : 248,
    "aux_sym_declaration_repeat1" : 249,
    "aux_sym_type_definition_repeat1" : 250,
    "aux_sym_type_definition_repeat2" : 251,
    "aux_sym__declaration_specifiers_repeat1" : 252,
    "aux_sym_attribute_declaration_repeat1" : 253,
    "aux_sym_attributed_declarator_repeat1" : 254,
    "aux_sym_pointer_declarator_repeat1" : 255,
    "aux_sym_function_declarator_repeat1" : 256,
    "aux_sym_sized_type_specifier_repeat1" : 257,
    "aux_sym_enumerator_list_repeat1" : 258,
    "aux_sym_field_declaration_repeat1" : 259,
    "aux_sym_parameter_list_repeat1" : 260,
    "aux_sym_case_statement_repeat1" : 261,
    "aux_sym_argument_list_repeat1" : 262,
    "aux_sym_initializer_list_repeat1" : 263,
    "aux_sym_initializer_pair_repeat1" : 264,
    "aux_sym_concatenated_string_repeat1" : 265,
    "aux_sym_string_literal_repeat1" : 266,
    "alias_sym_field_identifier" : 267,
    "alias_sym_statement_identifier" : 268,
    "alias_sym_type_identifier" : 269,
}

non_terminals = []

for elem in data:
    if ('children' in elem) or ('fields' in elem):
        assert f'sym_{elem["type"]}' in c_enums
        non_terminals.append((c_enums[f'sym_{elem["type"]}'], f'sym_{elem["type"]}'))

non_terminals.sort(key=lambda x: x[0])

table = [1] * 270

for a, b in non_terminals:
    table[a] = 0

table[  3] = 0 # anon_sym_LF
table[242] = 1 # sym_string_literal

str = ''
for i, t in enumerate(table):
    str += f'{t},'
    str += '\n' if i and ((i + 1) % 25 == 0) else ' '

print(str)