%{
#include "qfcc.h"
#include "expr.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1

extern char *yytext;
extern int lineno;

void
yyerror (const char *s)
{
	fprintf (stderr, "%d, %s %s\n", lineno, yytext, s);
}

int yylex (void);
type_t *PR_FindType (type_t *new);
type_t *parse_params (def_t *parms);

typedef struct {
	type_t	*type;
	def_t	*scope;
	def_t	*pscope;
} scope_t;

%}

%union {
	scope_t	scope;
	def_t	*def;
	type_t	*type;
	expr_t	*expr;
	int		int_val;
	float	float_val;
	char	*string_val;
	float	vector_val[3];
	float	quaternion_val[4];
	estatement_t *statement;
}

%left	OR AND
%right	'='
%left	EQ NE LE GE LT GT
%left	'+' '-'
%left	'*' '/' '&' '|'
%left	'!' '.'
%right	'('

%token	<string_val> NAME STRING_VAL
%token	<int_val> INT_VAL
%token	<float_val> FLOAT_VAL
%token	<vector_val> VECTOR_VAL
%token	<quaternion_val> QUATERNION_VAL

%token	LOCAL RETURN WHILE DO IF ELSE FOR ELIPSIS
%token	<type> TYPE

%type	<type>	type
%type	<def>	param param_list def_item def_list def_name
%type	<expr>	const expr arg_list opt_initializer opt_definition
%type	<statement>	statement statements statement_block

%expect 1

%{

type_t	*current_type;
def_t	*current_def;
def_t	param_scope;

%}

%%

defs
	: /* empty */
	| defs def ';'
	;

def
	: type
		{
			current_type = $1;
		}
	  def_list
		{
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
	: def_list ',' def_item
	| def_item
	;

def_item
	: def_name opt_initializer
		{
			$$ = $1;
		}
	| '('
		{
			$<scope>$.scope = pr_scope;
			$<scope>$.type = current_type;
			$<scope>$.pscope = param_scope.scope_next;
			param_scope.scope_next = 0;
			pr_scope = &param_scope;
		}
	  param_list
		{
			$$ = param_scope.scope_next;
			current_type = $<scope>2.type;
			param_scope.scope_next = $<scope>2.pscope;
			pr_scope = $<scope>2.scope;
		}
	  ')'
		{
			current_type = parse_params ($<def>4);
		}
	  def_name
		{
			$$ = $7->scope_next;
			$7->scope_next = $<def>4;
		}
	  opt_definition
	  	{
			PR_FlushScope ($7);
			$7->scope_next = $<def>8;

			$$ = $7;
		}
	| '(' ')'
		{
			current_type = parse_params (0);
		}
	  def_name
		{
			$$ = $4->scope_next;
			$4->scope_next = 0;
		}
	  opt_definition
		{
			$4->scope_next = $<def>5;
			$$ = $4;
		}
	| '(' ELIPSIS ')'
		{
			current_type = parse_params ((def_t*)1);
		}
	  def_name
		{
			$$ = $5->scope_next;
			$5->scope_next = 0;
		}
	  opt_definition
		{
			$5->scope_next = $<def>6;
			$$ = $5;
		}
	;

def_name
	: NAME
		{
			$$ = PR_GetDef (current_type, $1, pr_scope, pr_scope ? &pr_scope->num_locals : &numpr_globals);
			current_def = $$;
		}
	;

param_list
	: param
	| param_list ',' param
		{
			if ($3->next) {
				yyerror ("parameter redeclared");
				yyerror ($3->name);
				$$ = $1;
			} else {
				$3->next = $1;
				$$ = $3;
			}
		}
	;

param
	: type {current_type = $1;} def_item
		{
			$3->type = $1;
			$$ = $3;
		}
	;

opt_initializer
	: /*empty*/
		{
			$$ = 0;
		}
	| '=' const
		{
			$$ = $2;
		}
	;

opt_definition
	: /*empty*/
		{
			$$ = 0;
		}
	| '=' '#' const
		{
			if ($3->type != ex_int && $3->type != ex_float) {
				yyerror ("invalid constant for = #");
				$$ = 0;
			} else {
				$$ = $3;
			}
		}
	| '=' begin_function statement_block end_function
		{
			$$ = new_expr ();
			$$->type = ex_statement;
			$$->e.statement = $3;
		}
	| '=' '[' expr ',' expr ']' begin_function statement_block end_function
		{
			$$ = new_expr ();
			$$->type = ex_statement;
			$$->e.statement = $8;
		}
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
		{
			$$ = $2;
		}
	;

statements
	: /*empty*/
		{
			$$ = 0;
		}
	| statements statement
		{
/*			if ($1) {
				estatement_t *s = $1;
				while (s->next)
					s = s->next;
				s->next = $2;
				$$ = $1;
			} else { 
				$$ = $2;
			}*/
		}
	;

statement
	: ';' {}
	| statement_block { $$ = $1; }
	| RETURN expr ';' {}
	| RETURN ';' {}
	| WHILE '(' expr ')' statement {}
	| DO statement WHILE '(' expr ')' ';' {}
	| LOCAL type
		{
			current_type = $2;
		}
	  def_list ';' {}
	| IF '(' expr ')' statement {}
	| IF '(' expr ')' statement ELSE statement {}
	| FOR '(' expr ';' expr ';' expr ')' statement {}
	| expr ';' {}
	;

expr
	: expr AND expr	{ $$ = binary_expr (AND, $1, $3); }
	| expr OR expr	{ $$ = binary_expr (OR,  $1, $3); }
	| expr '=' expr	{ $$ = binary_expr ('=', $1, $3); }
	| expr EQ expr	{ $$ = binary_expr (EQ,  $1, $3); }
	| expr NE expr	{ $$ = binary_expr (NE,  $1, $3); }
	| expr LE expr	{ $$ = binary_expr (LE,  $1, $3); }
	| expr GE expr	{ $$ = binary_expr (GE,  $1, $3); }
	| expr LT expr	{ $$ = binary_expr (LT,  $1, $3); }
	| expr GT expr	{ $$ = binary_expr (GT,  $1, $3); }
	| expr '+' expr	{ $$ = binary_expr ('+', $1, $3); }
	| expr '-' expr	{ $$ = binary_expr ('-', $1, $3); }
	| expr '*' expr	{ $$ = binary_expr ('*', $1, $3); }
	| expr '/' expr	{ $$ = binary_expr ('/', $1, $3); }
	| expr '&' expr	{ $$ = binary_expr ('&', $1, $3); }
	| expr '|' expr	{ $$ = binary_expr ('|', $1, $3); }
	| expr '.' expr	{ $$ = binary_expr ('.', $1, $3); }
	| expr '(' arg_list ')'	{ $$ = function_expr ($1, $3); }
	| expr '(' ')'			{ $$ = function_expr ($1, 0); }
	| '-' expr		{ $$ = unary_expr ('-', $2); }
	| '!' expr		{ $$ = unary_expr ('!', $2); }
	| NAME
		{
			$$ = new_expr ();
			$$->type = ex_def;
			$$->e.def = PR_GetDef (NULL, $1, pr_scope, false);
		}
	| const			{ $$ = $1; }
	| '(' expr ')'	{ $$ = $2; }
	;

arg_list
	: expr
	| arg_list ',' expr
		{
			$3->next = $1;
			$$ = $3;
		}
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

type_t *
parse_params (def_t *parms)
{
	type_t		new;
	def_t		*p;
	int			i;

	memset (&new, 0, sizeof (new));
	new.type = ev_func;
	new.aux_type = current_type;
	new.num_parms = 0;

	if (!parms) {
	} else if (parms == (def_t*)1) {
		new.num_parms = -1;			// variable args
	} else {
		for (p = parms; p; p = p->next, new.num_parms++)
			;
		if (new.num_parms > MAX_PARMS) {
			yyerror ("too many params");
			return current_type;
		}
		i = 1;
		do {
			//puts (parms->name);
			strcpy (pr_parm_names[new.num_parms - i], parms->name);
			new.parm_types[new.num_parms - i] = parms->type;
			i++;
			parms = parms->next;
		} while (parms);
	}
	return PR_FindType (&new);
}
