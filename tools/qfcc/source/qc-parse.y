%{
#include "qfcc.h"
#include "expr.h"

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
	expr_t	*expr;
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
%type	<def>	param param_list def_item def_list def_name
%type	<expr>	const expr arg_list

%expect 1

%{

type_t	*current_type;
def_t	*current_def;

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
	: def_name opt_initializer
	| '(' param_list ')' def_name opt_definition {}
	| '(' ')' def_name opt_definition {}
	| '(' ELIPSIS ')' def_name opt_definition {}
	;

def_name
	: NAME
		{
			printf ("%s\n", $1);
			$$ = PR_GetDef (current_type, $1, pr_scope, pr_scope ? &pr_scope->num_locals : &numpr_globals);
			current_def = $$;
		}
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
	| '=' const
	;

opt_definition
	: /*empty*/
	| '=' '#' const
		{
			if ($3->type != ex_int && $3->type != ex_float)
				yyerror ("invalid constant for = #");
			else {
			}
		}
	| '=' begin_function statement_block end_function
	| '=' '[' expr ',' expr ']' begin_function statement_block end_function
	;

begin_function
	: /*empty*/
		{
			pr_scope = current_def;
		}
	;

end_function
	: /*empty*/
		{
			pr_scope = 0;
		}
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
	| expr ';' {}
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
	| '-' expr {}
	| '!' expr {}
	| NAME
		{
			$$ = new_expr ();
			$$->type = ex_def;
			$$->e.def = PR_GetDef (NULL, $1, pr_scope, false);
		}
	| const
	| '(' expr ')' { $$ = $2; }
	;

arg_list
	: expr
	| arg_list ',' expr
	;

const
	: FLOAT_VAL
		{
			$$ = new_expr ();
			$$->type = ex_float;
			$$->e.float_val = $1;
		}
	| STRING_VAL {}
		{
			$$ = new_expr ();
			$$->type = ex_string;
			$$->e.string_val = ReuseString ($1);
		}
	| VECTOR_VAL {}
		{
			$$ = new_expr ();
			$$->type = ex_vector;
			memcpy ($$->e.vector_val, $1, sizeof ($$->e.vector_val));
		}
	| QUATERNION_VAL {}
		{
			$$ = new_expr ();
			$$->type = ex_quaternion;
			memcpy ($$->e.quaternion_val, $1, sizeof ($$->e.quaternion_val));
		}
	| INT_VAL {}
		{
			$$ = new_expr ();
			$$->type = ex_int;
			$$->e.int_val = $1;
		}
	;

%%
