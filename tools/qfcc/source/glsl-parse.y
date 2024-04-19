/*
	glsl-parse.y

	parser for GLSL

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/11/25

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
%define api.prefix {glsl_yy}
%define api.pure full
%define api.push-pull push
%define api.token.prefix {GLSL_}
%locations
%parse-param {void *scanner}
%define api.value.type {rua_val_t}
%define api.location.type {rua_loc_t}

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/hash.h>
#include <QF/sys.h>
#include <QF/va.h>

#define GLSL_YYDEBUG 1
#define GLSL_YYERROR_VERBOSE 1
#undef GLSL_YYERROR_VERBOSE

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/glsl-parse.h"

#define glsl_yytext qc_yyget_text (scanner)
char *qc_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, void *scanner, const char *s)
{
#ifdef GLSL_YYERROR_VERBOSE
	error (0, "%s %s\n", glsl_yytext, s);
#else
	error (0, "%s before %s", s, glsl_yytext);
#endif
}

static void __attribute__((used))
parse_error (void *scanner)
{
	error (0, "parse error before %s", glsl_yytext);
}

#define PARSE_ERROR do { parse_error (scanner); YYERROR; } while (0)

#define first_line line
#define first_column column

int yylex (YYSTYPE *yylval, YYLTYPE *yylloc);

%}

%code requires { #define glsl_yypstate rua_yypstate }

// these tokens are common between qc and qp
%left LOW
%nonassoc IFX
%nonassoc ELSE
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY
%nonassoc STORAGEX

%left			COMMA
%right	<op>	'=' ASX
%right			'?' ':'
%left			OR
%left			XOR
%left			AND
%left			'|'
%left			'^'
%left			'&'
%left			EQ NE
%left			LT GT GE LE
%token			NAND NOR XNOR
// end of tokens common between qc and qp

%left			SHL SHR
%left			'+' '-'
%left			'*' '/' '%' MOD SCALE GEOMETRIC QMUL QVMUL VQMUL
%left           HADAMARD CROSS DOT WEDGE REGRESSIVE
%right	<op>	SIZEOF UNARY INCOP REVERSE STAR DUAL UNDUAL
%left			HYPERUNARY
%left			'.' '(' '['

%token	<expr>		VALUE STRING TOKEN
%token              ELLIPSIS
%token              RESERVED
// end of common tokens

%token	<symbol>	IDENTIFIER

%token				WHILE DO IF ELSE FOR BREAK CONTINUE
%token				RETURN
%token				SWITCH CASE DEFAULT
%token	<op>		STRUCT
%token	<spec>		TYPE_SPEC TYPE_NAME TYPE_QUAL VOID

%token PRECISION INVARIANT SMOOTH FLAT NOPERSPECTIVE LAYOUT SHARED
%token PRECISE CONST IN OUT INOUT CENTROID PATCH SAMPLE UNIFORM BUFFER VOLATILE
%token RESTRICT READONLY WRITEONLY HIGH_PRECISION MEDIUM_PRECISION
%token LOW_PRECISION DISCARD COHERENT

%{
%}

%expect 0

%%

translation_unit
	: external_declaration
	| translation_unit external_declaration
	;

external_declaration
	: function_definition
	| declaration
	| ';'
	;

function_definition
	: function_prototype compound_statement_no_new_scope
	;

variable_identifier
	: IDENTIFIER
	;

primary_exprsssion
	: variable_identifier
	| VALUE
	| '(' expression ')'
	;

postfix_expression
	: primary_exprsssion
	| postfix_expression '[' integer_expression ']'
	| function_call
	| postfix_expression '.' IDENTIFIER/*FIELD_SELECTION*/
	| postfix_expression INCOP
	;

integer_expression
	: expression
	;

function_call
	: function_call_or_method
	;

function_call_or_method
	: function_call_generic
	;

function_call_generic
	: function_call_header_with_parameters ')'
	| function_call_header_no_parameters ')'
	;

function_call_header_no_parameters
	: function_call_header VOID
	| function_call_header
	;

function_call_header_with_parameters
	: function_call_header assignment_expression
	| function_call_header_with_parameters ',' assignment_expression
	;

function_call_header
	: function_identifier '('
	;

function_identifier
	: type_specifier
	| postfix_expression
	;

unary_expression
	: postfix_expression
	| INCOP unary_expression
	| unary_operator unary_expression
	;

unary_operator
	: '+'
	| '-'
	| '!'
	| '~'
	;

multiplicative_expression
	: unary_expression
	| multiplicative_expression '*' unary_expression
	| multiplicative_expression '/' unary_expression
	| multiplicative_expression '%' unary_expression
	;

additive_expression
	: multiplicative_expression
	| additive_expression '+' unary_expression
	| additive_expression '-' unary_expression
	;

shift_expression
	: additive_expression
	| shift_expression SHL additive_expression
	| shift_expression SHR additive_expression
	;

relational_expression
	: shift_expression
	| relational_expression LT shift_expression
	| relational_expression GT shift_expression
	| relational_expression LE shift_expression
	| relational_expression GE shift_expression
	;

equality_expression
	: relational_expression
	| equality_expression EQ relational_expression
	| relational_expression NE relational_expression
	;

and_expression
	: equality_expression
	| and_expression '&' equality_expression
	;

exclusive_or_expression
	: and_expression
	| exclusive_or_expression '^' and_expression
	;

inclusive_or_expression
	: exclusive_or_expression
	| inclusive_or_expression '|' exclusive_or_expression
	;

logical_and_expression
	: inclusive_or_expression
	| logical_and_expression AND inclusive_or_expression
	;

logical_xor_expression
	: logical_and_expression
	| logical_xor_expression XOR logical_and_expression
	;

logical_or_expression
	: logical_xor_expression
	| logical_or_expression OR logical_xor_expression
	;

conditional_expression
	: logical_or_expression
	| logical_or_expression '?' expression ':' assignment_expression
	;

assignment_expression
	: conditional_expression
	| unary_expression assignment_operator assignment_expression
	;

assignment_operator
	: '='
	| ASX
	;

expression
	: assignment_expression
	| expression ',' assignment_expression
	;

constant_expression
	: conditional_expression
	;

declaration
	: function_prototype ';'
	| init_declarator_list ';'
	| PRECISION precision_qualifier type_specifier ';'
	| type_qualifier IDENTIFIER '{' struct_declaration_list '}' ';'
	| type_qualifier IDENTIFIER '{' struct_declaration_list '}' IDENTIFIER ';'
	| type_qualifier IDENTIFIER '{' struct_declaration_list '}' IDENTIFIER array_specifier ';'
	| type_qualifier ';'
	| type_qualifier IDENTIFIER ';'
	| type_qualifier IDENTIFIER identifier_list ';'
	;

identifier_list
	: ',' IDENTIFIER
	| identifier_list ',' IDENTIFIER
	;

function_prototype
	: function_declarator ')'
	;

function_declarator
	: function_header
	| function_header_with_parameters
	;

function_header_with_parameters
	: function_header parameter_declaration
	| function_header_with_parameters ',' parameter_declaration
	;

function_header
	: fully_specified_type IDENTIFIER '('
	;

parameter_declarator
	: type_specifier IDENTIFIER
	| type_specifier IDENTIFIER array_specifier
	;

parameter_declaration
	: type_qualifier parameter_declarator
	| parameter_declarator
	| type_qualifier parameter_type_specifier
	| parameter_type_specifier
	;

parameter_type_specifier
	: type_specifier
	;

init_declarator_list
	: single_declaration
	| init_declarator_list ',' IDENTIFIER
	| init_declarator_list ',' IDENTIFIER array_specifier
	| init_declarator_list ',' IDENTIFIER array_specifier '=' initializer
	| init_declarator_list ',' IDENTIFIER '=' initializer
	;

single_declaration
	: fully_specified_type
	| fully_specified_type IDENTIFIER
	| fully_specified_type IDENTIFIER array_specifier
	| fully_specified_type IDENTIFIER array_specifier '=' initializer
	| fully_specified_type IDENTIFIER '=' initializer
	;

fully_specified_type
	: type_specifier
	| type_qualifier type_specifier
	;

invariant_qualifier
	: INVARIANT
	;

interpolation_qualifier
	: SMOOTH
	| FLAT
	| NOPERSPECTIVE
	;

layout_qualifier
	: LAYOUT '(' layout_qualifer_id_list ')'
	;

layout_qualifer_id_list
	: layout_qualifer_id
	| layout_qualifer_id_list ',' layout_qualifer_id
	;

layout_qualifer_id
	: IDENTIFIER
	| IDENTIFIER '=' constant_expression
	| SHARED
	;

precise_qualifer
	: PRECISE
	;

type_qualifier
	: single_type_qualifier
	| type_qualifier single_type_qualifier
	;

single_type_qualifier
	: storage_qualifier
	| layout_qualifier
	| precision_qualifier
	| interpolation_qualifier
	| invariant_qualifier
	| precise_qualifer
	;

storage_qualifier
	: CONST
	| IN
	| OUT
	| INOUT
	| CENTROID
	| PATCH
	| SAMPLE
	| UNIFORM
	| BUFFER
	| SHARED
	| COHERENT
	| VOLATILE
	| RESTRICT
	| READONLY
	| WRITEONLY
	;

type_specifier
	: type_specifier_nonarray
	| type_specifier_nonarray array_specifier
	;

array_specifier
	: '[' ']'
	| '[' conditional_expression  ']'
	| array_specifier '[' ']'
	| array_specifier '[' conditional_expression ']'
	;

type_specifier_nonarray
	: VOID
	| TYPE_SPEC
	| struct_specifier
	| TYPE_NAME
	;

precision_qualifier
	: HIGH_PRECISION
	| MEDIUM_PRECISION
	| LOW_PRECISION
	;

struct_specifier
	: STRUCT IDENTIFIER '{' struct_declaration_list '}'
	| STRUCT '{' struct_declaration_list '}'
	;

struct_declaration_list
	: struct_declaration
	| struct_declaration_list struct_declaration
	;

struct_declaration
	: type_specifier struct_declarator_list ';'
	| type_qualifier type_specifier struct_declarator_list ';'
	;

struct_declarator_list
	: struct_declarator
	| struct_declarator_list ',' struct_declarator
	;

struct_declarator
	: IDENTIFIER
	| IDENTIFIER array_specifier
	;

initializer
	: assignment_expression
	| '{' initializer_list '}'
	| '{' initializer_list ',' '}'
	;

initializer_list
	: initializer
	| initializer_list ',' initializer
	;

declaration_statement
	: declaration
	;

statement
	: compound_statement
	| simple_statement
	;

simple_statement
	: declaration_statement
	| expression_statement
	| selection_statement
	| switch_statement
	| case_label
	| iteration_statement
	| jump_statement
	;

compound_statement
	: '{' '}'
	| '{' statement_list '}'
	;

statement_no_new_scope
	: compound_statement_no_new_scope
	| simple_statement
	;

compound_statement_no_new_scope
	: '{' '}'
	| '{' statement_list '}'
	;

statement_list
	: statement
	| statement_list statement
	;

expression_statement
	: ';'
	| expression ';'
	;

selection_statement
	: IF '(' expression ')' selection_rest_statement
	;

selection_rest_statement
	: statement ELSE statement
	| statement %prec IFX
	;

condition
	: expression
	| fully_specified_type IDENTIFIER '=' initializer
	;

switch_statement
	: SWITCH '(' expression ')' '{' switch_statement_list '}'
	;

switch_statement_list
	: /* empty */
	| statement_list
	;

case_label
	: CASE expression ':'
	| DEFAULT ':'
	;

iteration_statement
	: WHILE '(' condition ')' statement_no_new_scope
	| DO statement WHILE '(' expression ')' ';'
	| FOR '(' for_init_statement for_rest_statement ')' statement_no_new_scope
	;

for_init_statement
	: expression_statement
	| declaration_statement
	;

conditionopt
	: condition
	| /* emtpy */
	;

for_rest_statement
	: conditionopt ';'
	| conditionopt ';' expression
	;

jump_statement
	: CONTINUE ';'
	| BREAK ';'
	| RETURN ';'
	| RETURN expression ';'
	| DISCARD ';'
	;

%%


static keyword_t glsl_keywords[] = {
	{"const",           GLSL_CONST},
	{"uniform",         GLSL_UNIFORM},
	{"buffer",          GLSL_BUFFER},
	{"shared",          GLSL_SHARED},
	{"attribute",       0},
	{"varying",         0},
	{"coherent",        GLSL_COHERENT},
	{"volatile",        GLSL_VOLATILE},
	{"restrict",        GLSL_RESTRICT},
	{"readonly",        GLSL_READONLY},
	{"writeonly",       GLSL_WRITEONLY},
	{"atomic_uint",     0},
	{"layout",          GLSL_LAYOUT},
	{"centroid",        GLSL_CENTROID},
	{"flat",            GLSL_FLAT},
	{"smooth",          GLSL_SMOOTH},
	{"noperspective",   GLSL_NOPERSPECTIVE},
	{"patch",           GLSL_PATCH},
	{"sample",          GLSL_SAMPLE},
	{"invariant",       GLSL_INVARIANT},
	{"precise",         GLSL_PRECISE},
	{"break",           GLSL_BREAK},
	{"continue",        GLSL_CONTINUE},
	{"do",              GLSL_DO},
	{"for",             GLSL_FOR},
	{"while",           GLSL_WHILE},
	{"switch",          GLSL_SWITCH},
	{"case",            GLSL_CASE},
	{"default",         GLSL_DEFAULT},
	{"if",              GLSL_IF},
	{"else",            GLSL_ELSE},
	{"subroutine",      GLSL_RESERVED},
	{"in",              GLSL_IN},
	{"out",             GLSL_OUT},
	{"inout",           GLSL_INOUT},
	{"int",             GLSL_TYPE_SPEC, .spec = {.type = &type_int}},
	{"void",            GLSL_VOID, .spec = {.type = &type_void}},
	{"bool",            GLSL_TYPE_SPEC, .spec = {.type = &type_bool}},
	{"true",            0},
	{"false",           0},
	{"float",           GLSL_TYPE_SPEC, .spec = {.type = &type_float}},
	{"double",          GLSL_TYPE_SPEC, .spec = {.type = &type_double}},
	{"discard",         0},
	{"return",          GLSL_RETURN},
	{"vec2",            GLSL_TYPE_SPEC, .spec = {.type = &type_vec2}},
	{"vec3",            GLSL_TYPE_SPEC, .spec = {.type = &type_vec3}},
	{"vec4",            GLSL_TYPE_SPEC, .spec = {.type = &type_vec4}},
	{"ivec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_ivec2}},
	{"ivec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_ivec3}},
	{"ivec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_ivec4}},
	{"bvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_bvec2}},
	{"bvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_bvec3}},
	{"bvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_bvec4}},
	{"uint",            GLSL_TYPE_SPEC, .spec = {.type = &type_uint}},
	{"uvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_uivec2}},
	{"uvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_uivec3}},
	{"uvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_uivec4}},
	{"dvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_dvec2}},
	{"dvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_dvec3}},
	{"dvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_dvec4}},
	{"mat2",            GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x2}},
	{"mat3",            GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x3}},
	{"mat4",            GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x4}},
	{"mat2x2",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x2}},
	{"mat2x3",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x3}},
	{"mat2x4",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x4}},
	{"mat3x2",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x2}},
	{"mat3x3",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x3}},
	{"mat3x4",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x4}},
	{"mat4x2",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x2}},
	{"mat4x3",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x3}},
	{"mat4x4",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x4}},
	{"dmat2",           GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x2}},
	{"dmat3",           GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x3}},
	{"dmat4",           GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x4}},
	{"dmat2x2",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x2}},
	{"dmat2x3",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x3}},
	{"dmat2x4",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x4}},
	{"dmat3x2",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x2}},
	{"dmat3x3",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x3}},
	{"dmat3x4",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x4}},
	{"dmat4x2",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x2}},
	{"dmat4x3",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x3}},
	{"dmat4x4",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x4}},

	{"lowp",                    GLSL_LOW_PRECISION},
	{"mediump",                 GLSL_MEDIUM_PRECISION},
	{"highp",                   GLSL_HIGH_PRECISION},
	{"precision",               GLSL_PRECISION},
	{"sampler1D",               0},
	{"sampler1DShadow",         0},
	{"sampler1DArray",          0},
	{"sampler1DArrayShadow",    0},
	{"isampler1D",              0},
	{"isampler1DArray",         0},
	{"usampler1D",              0},
	{"usampler1DArray",         0},
	{"sampler2D",               0},
	{"sampler2DShadow",         0},
	{"sampler2DArray",          0},
	{"sampler2DArrayShadow",    0},
	{"isampler2D",              0},
	{"isampler2DArray",         0},
	{"usampler2D",              0},
	{"usampler2DArray",         0},
	{"sampler2DRect",           0},
	{"sampler2DRectShadow",     0},
	{"isampler2DRect",          0},
	{"usampler2DRect",          0},
	{"sampler2DMS",             0},
	{"isampler2DMS",            0},
	{"usampler2DMS",            0},
	{"sampler2DMSArray",        0},
	{"isampler2DMSArray",       0},
	{"usampler2DMSArray",       0},
	{"sampler3D",               0},
	{"isampler3D",              0},
	{"usampler3D",              0},
	{"samplerCube",             0},
	{"samplerCubeShadow",       0},
	{"isamplerCube",            0},
	{"usamplerCube",            0},
	{"samplerCubeArray",        0},
	{"samplerCubeArrayShadow",  0},
	{"isamplerCubeArray",       0},
	{"usamplerCubeArray",       0},
	{"samplerBuffer",           0},
	{"isamplerBuffer",          0},
	{"usamplerBuffer",          0},
	{"image1D",                 0},
	{"iimage1D",                0},
	{"uimage1D",                0},
	{"image1DArray",            0},
	{"iimage1DArray",           0},
	{"uimage1DArray",           0},
	{"image2D",                 0},
	{"iimage2D",                0},
	{"uimage2D",                0},
	{"image2DArray",            0},
	{"iimage2DArray",           0},
	{"uimage2DArray",           0},
	{"image2DRect",             0},
	{"iimage2DRect",            0},
	{"uimage2DRect",            0},
	{"image2DMS",               0},
	{"iimage2DMS",              0},
	{"uimage2DMS",              0},
	{"image2DMSArray",          0},
	{"iimage2DMSArray",         0},
	{"uimage2DMSArray",         0},
	{"image3D",                 0},
	{"iimage3D",                0},
	{"uimage3D",                0},
	{"imageCube",               0},
	{"iimageCube",              0},
	{"uimageCube",              0},
	{"imageCubeArray",          0},
	{"iimageCubeArray",         0},
	{"uimageCubeArray",         0},
	{"imageBuffer",             0},
	{"iimageBuffer",            0},
	{"uimageBuffer",            0},

	{"struct",                  GLSL_STRUCT},

	//vulkan
	{"texture1D",               0},
	{"texture1DArray",          0},
	{"itexture1D",              0},
	{"itexture1DArray",         0},
	{"utexture1D",              0},
	{"utexture1DArray",         0},
	{"texture2D",               0},
	{"texture2DArray",          0},
	{"itexture2D",              0},
	{"itexture2DArray",         0},
	{"utexture2D",              0},
	{"utexture2DArray",         0},
	{"texture2DRect",           0},
	{"itexture2DRect",          0},
	{"utexture2DRect",          0},
	{"texture2DMS",             0},
	{"itexture2DMS",            0},
	{"utexture2DMS",            0},
	{"texture2DMSArray",        0},
	{"itexture2DMSArray",       0},
	{"utexture2DMSArray",       0},
	{"texture3D",               0},
	{"itexture3D",              0},
	{"utexture3D",              0},
	{"textureCube",             0},
	{"itextureCube",            0},
	{"utextureCube",            0},
	{"textureCubeArray",        0},
	{"itextureCubeArray",       0},
	{"utextureCubeArray",       0},
	{"textureBuffer",           0},
	{"itextureBuffer",          0},
	{"utextureBuffer",          0},
	{"sampler",                 0},
	{"samplerShadow",           0},
	{"subpassInput",            0},
	{"isubpassInput",           0},
	{"usubpassInput",           0},
	{"subpassInputMS",          0},
	{"isubpassInputMS",         0},
	{"usubpassInputMS",         0},

	//reserved
	{"common",          GLSL_RESERVED},
	{"partition",       GLSL_RESERVED},
	{"active",          GLSL_RESERVED},
	{"asm",             GLSL_RESERVED},
	{"class",           GLSL_RESERVED},
	{"union",           GLSL_RESERVED},
	{"enum",            GLSL_RESERVED},
	{"typedef",         GLSL_RESERVED},
	{"template",        GLSL_RESERVED},
	{"this",            GLSL_RESERVED},
	{"resource",        GLSL_RESERVED},
	{"goto",            GLSL_RESERVED},
	{"inline",          GLSL_RESERVED},
	{"noinline",        GLSL_RESERVED},
	{"public",          GLSL_RESERVED},
	{"static",          GLSL_RESERVED},
	{"extern",          GLSL_RESERVED},
	{"external",        GLSL_RESERVED},
	{"interface",       GLSL_RESERVED},
	{"long",            GLSL_RESERVED},
	{"short",           GLSL_RESERVED},
	{"half",            GLSL_RESERVED},
	{"fixed",           GLSL_RESERVED},
	{"unsigned",        GLSL_RESERVED},
	{"superp",          GLSL_RESERVED},
	{"input",           GLSL_RESERVED},
	{"output",          GLSL_RESERVED},
	{"hvec2",           GLSL_RESERVED},
	{"hvec3",           GLSL_RESERVED},
	{"hvec4",           GLSL_RESERVED},
	{"fvec2",           GLSL_RESERVED},
	{"fvec3",           GLSL_RESERVED},
	{"fvec4",           GLSL_RESERVED},
	{"filter",          GLSL_RESERVED},
	{"sizeof",          GLSL_RESERVED},
	{"cast",            GLSL_RESERVED},
	{"namespace",       GLSL_RESERVED},
	{"using",           GLSL_RESERVED},
	{"sampler3DRect",   GLSL_RESERVED},
};

// preprocessor directives in ruamoko and quakec
static directive_t glsl_directives[] = {
	{"include",     PRE_INCLUDE},
	{"define",      PRE_DEFINE},
	{"undef",       PRE_UNDEF},
	{"error",       PRE_ERROR},
	{"warning",     PRE_WARNING},
	{"notice",      PRE_NOTICE},
	{"pragma",      PRE_PRAGMA},
	{"line",        PRE_LINE},

	{"extension",   PRE_EXTENSION},
	{"version",     PRE_VERSION},
};

static directive_t *
glsl_directive (const char *token)
{
	static hashtab_t *directive_tab;

	if (!directive_tab) {
		directive_tab = Hash_NewTable (253, rua_directive_get_key, 0, 0, 0);
		for (size_t i = 0; i < ARRCOUNT(glsl_directives); i++) {
			Hash_Add (directive_tab, &glsl_directives[i]);
		}
	}
	directive_t *directive = Hash_Find (directive_tab, token);
	return directive;
}

static int
glsl_process_keyword (rua_val_t *lval, keyword_t *keyword, const char *token)
{
	if (keyword->value == GLSL_STRUCT) {
		lval->op = token[0];
	} else {
		lval->spec = keyword->spec;
	}
	return keyword->value;
}

static int
glsl_keyword_or_id (rua_val_t *lval, const char *token)
{
	static hashtab_t *glsl_keyword_tab;

	keyword_t  *keyword = 0;
	symbol_t   *sym;

	if (!glsl_keyword_tab) {
		size_t      i;

		glsl_keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);

		for (i = 0; i < ARRCOUNT(glsl_keywords); i++)
			Hash_Add (glsl_keyword_tab, &glsl_keywords[i]);
	}
	keyword = Hash_Find (glsl_keyword_tab, token);
	if (keyword && keyword->value)
		return glsl_process_keyword (lval, keyword, token);
	if (token[0] == '@') {
		return '@';
	}
	sym = symtab_lookup (current_symtab, token);
	if (!sym)
		sym = new_symbol (token);
	lval->symbol = sym;
	if (sym->sy_type == sy_type) {
		lval->spec = (specifier_t) {
			.type = sym->type,
			.sym = sym,
		};
		return GLSL_TYPE_NAME;
	}
	return GLSL_IDENTIFIER;
}

int
glsl_yyparse (FILE *in)
{
	rua_parser_t parser = {
		.parse = glsl_yypush_parse,
		.state = glsl_yypstate_new (),
		.directive = glsl_directive,
		.keyword_or_id = glsl_keyword_or_id,
	};
	int ret = rua_parse (in, &parser);
	glsl_yypstate_delete (parser.state);
	return ret;
}
