%{
#include "qfcc.h"
#define YYDEBUG 1
#define YYERROR_VERBOSE 1
void
yyerror (char *s)
{
	extern int lineno;
	extern char *yytext;
	fprintf (stderr, "%d, %s %s\n", lineno, yytext, s);
}

int yylex (void);
type_t *PR_FindType (type_t *new);

%}

%union {
	def_t	*def;
	type_t	*type;
	int		int_val;
	float	float_val;
	char	*string_val;
	float	vector_val[3];
	float	quaternion_val[4];
}

%left	OR AND
%right	'='
%left	EQ NE LE GE LT GT
%left	'+' '-'
%left	'*' '/' '&' '|'
%left	'!' '.' '('

%token	<string_val> NAME STRING_VAL
%token	<int_val> INT_VAL
%token	<float_val> FLOAT_VAL
%token	<vector_val> VECTOR_VAL
%token	<quaternion_val> QUATERNION_VAL

%token	LOCAL RETURN WHILE DO IF ELSE FOR ELIPSIS
%token	<type> TYPE

%type	<type>	type
%type	<def>	param param_list def_item def_list expr arg_list

%expect 1

%{

type_t *current_type;

%}

%%

defs
	: /* empty */
	| defs def ';'
	;

def
	: type {current_type = $1;} def_list {}
	;

type
	: TYPE
	| '.' TYPE
		{
			type_t new;
			memset (&new, 0, sizeof (new));
			new.type = ev_field;
			new.aux_type = $2;
			$$ = PR_FindType (&new);
		}
	;

def_list
	: def_item ',' def_list
		{
			$1->next = $3;
			$$ = $1;
		}
	| def_item
	;

def_item
	: NAME
		{
			$$ = PR_GetDef (current_type, $1, pr_scope, pr_scope ? &pr_scope->num_locals : &numpr_globals);
		}
	  opt_initializer
	| '(' param_list ')' NAME opt_initializer {}
	| '(' ')' NAME opt_initializer {}
	| '(' ELIPSIS ')' NAME opt_initializer {}
	;

param_list
	: param
	| param_list ',' param
		{
			$1->next = $3;
			$$ = $1;
		}
	;

param
	: type def_item
		{
			$2->type = $1;
			$$ = $2;
		}
	;

opt_initializer
	: /*empty*/
	| '=' '#' const
	| '=' statement_block
	| '=' const
	| '=' '[' expr ',' expr ']' statement_block
	;

statement_block
	: '{' statements '}'
	;

statements
	: /*empty*/
	| statements statement
	;

statement
	: ';'
	| statement_block
	| RETURN expr ';'
	| RETURN ';'
	| WHILE '(' expr ')' statement
	| DO statement WHILE '(' expr ')' ';'
	| LOCAL type def_list ';'
	| IF '(' expr ')' statement
	| IF '(' expr ')' statement ELSE statement
	| FOR '(' expr ';' expr ';' expr ')' statement
	| expr ';'
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
	| expr '(' ')'
	| '-' expr
	| '!' expr
	| NAME
		{
			$$ = PR_GetDef (NULL, $1, pr_scope, false);
		}
	| const {}
	| '(' expr ')' { $$ = $2; }
	;

arg_list
	: expr
	| arg_list ',' expr
	;

const
	: FLOAT_VAL {}
	| STRING_VAL {}
	| VECTOR_VAL {}
	| QUATERNION_VAL {}
	| INT_VAL {}
	;

%%
