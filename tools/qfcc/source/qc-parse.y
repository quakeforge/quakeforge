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

%token	NAME INT_VAL FLOAT_VAL STRING_VAL VECTOR_VAL QUATERNION_VAL

%token	LOCAL RETURN WHILE DO IF ELSE FOR ELIPSIS
%token	<type> TYPE

%type	<type>	type
%type	<def>	param param_list def_item def def_list

%expect 1

%%

defs
	: /* empty */
	| defs def ';'
	;

def
	: type def_list
		{
			def_t *def;
			for (def = $2; def; def = def->next)
				def->type = $1;
			$$ = $2;
		}
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
	: NAME opt_initializer {}
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
	| const
	| '(' expr ')'
	;

arg_list
	: expr
	| arg_list ',' expr
	;

const
	: FLOAT_VAL
	| STRING_VAL
	| VECTOR_VAL
	| QUATERNION_VAL
	| INT_VAL
	;

%%
