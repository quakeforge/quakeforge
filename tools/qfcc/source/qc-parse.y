%{
#include "qfcc.h"

void
yyerror (char *s)
{
	fprintf (stderr, "%s\n", s);
}

int yylex (void);

%}

%union {
	def_t	*def;
}

%left	OR AND
%right	'='
%left	EQ NE LE GE LT GT
%left	'+' '-'
%left	'*' '/' '&' '|'
%left	'!' '.' '('

%token	NAME IMMEDIATE

%token	LOCAL TYPE RETURN WHILE DO IF ELSE FOR

%expect 1

%%

defs
	: /* empty */
	| defs def ';'
	;

def
	: TYPE def_list
	| '.' TYPE def_list
	;

def_list
	: def_item ',' def_list
	| def_item
	;

def_item
	: NAME opt_initializer
	| '(' param_list ')' NAME opt_initializer
	;

param_list
	: /* emtpy */
	| param_list ',' param
	;

param
	: TYPE NAME
	;

opt_initializer
	: /*empty*/
	| '=' '#' IMMEDIATE
	| '=' statement_block
	| '=' IMMEDIATE
	| '=' '[' expr ',' expr ']' statement_block
	;

statement_block
	: '{' statements '}'
	;

statements
	: /*empty*/
	| statements statement ';'
	;

statement
	: ';'
	| statement_block
	| RETURN expr
	| RETURN
	| WHILE '(' expr ')' statement
	| DO statement WHILE '(' expr ')'
	| LOCAL def_list
	| IF '(' expr ')' statement
	| IF '(' expr ')' statement ELSE statement
	| FOR '(' expr ';' expr ';' expr ')' statement
	| expr
	;

expr
	: expr AND expr
	| expr OR expr
	| expr '=' expr
	| expr EQ expr
	| expr NE expr
	| expr LE expr
	| expr GE expr
	| expr LT expr
	| expr GT expr
	| expr '+' expr
	| expr '-' expr
	| expr '*' expr
	| expr '/' expr
	| expr '&' expr
	| expr '|' expr
	| expr '.' expr
	| expr '(' arg_list ')'
	| '!' expr
	| NAME
	| IMMEDIATE
	| '(' expr ')'
	;

arg_list
	: /*empty*/
	| arg_list ',' expr
	;

%%
