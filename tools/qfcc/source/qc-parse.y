%{
#include "qfcc.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1

extern char *yytext;
extern int pr_source_line;

void
yyerror (const char *s)
{
	error (0, "%s %s\n", yytext, s);
}

int yylex (void);
type_t *PR_FindType (type_t *new);
void PR_PrintType(type_t*);

type_t *parse_params (def_t *parms);
function_t *new_function (void);
void build_function (function_t *f);
void finish_function (function_t *f);
void emit_function (function_t *f, expr_t *e);
void build_scope (function_t *f, def_t *func);

typedef struct {
	type_t	*type;
	def_t	*scope;
	def_t	*pscope;
} scope_t;

%}

%union {
	int		op;
	scope_t	scope;
	def_t	*def;
	type_t	*type;
	expr_t	*expr;
	int		integer_val;
	float	float_val;
	char	*string_val;
	float	vector_val[3];
	float	quaternion_val[4];
	function_t *function;
}

%right	<op> '=' ASX
%right	'?' ':'
%left	OR AND
%left	EQ NE LE GE LT GT
%left	SHL SHR
%left	'+' '-'
%left	'*' '/' '&' '|' '^' '%'
//%left	'!' '~'
%right	<op> UNARY INCOP
%right	'('
%left	'.'

%token	<string_val> NAME STRING_VAL
%token	<integer_val> INT_VAL
%token	<float_val> FLOAT_VAL
%token	<vector_val> VECTOR_VAL
%token	<quaternion_val> QUATERNION_VAL

%token	LOCAL RETURN WHILE DO IF ELSE FOR ELIPSIS
%token	<type> TYPE

%type	<type>	type maybe_func
%type	<def>	param param_list def_item def_list def_name
%type	<expr>	const opt_expr expr arg_list
%type	<expr>	statement statements statement_block
%type	<function> begin_function

%expect 1

%{

type_t	*current_type;
def_t	*current_def;
def_t	param_scope;
function_t *current_func;
expr_t	*local_expr;

def_t		*pr_scope;					// the function being parsed, or NULL
string_t	s_file;						// filename for function definition

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
		{
			current_type = $1;
		}
	  maybe_func
	  	{
			$$ = $3 ? $3 : $1;
		}
	| '.' TYPE
		{
			current_type = $2;
		}
	  maybe_func
		{
			type_t new;
			memset (&new, 0, sizeof (new));
			new.type = ev_field;
			new.aux_type = $4 ? $4 : $2;
			$$ = PR_FindType (&new);
		}
	;

maybe_func
	: /* empty */
		{
			$$ = 0;
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
			PR_FlushScope (&param_scope);
			current_type = $<scope>2.type;
			param_scope.scope_next = $<scope>2.pscope;
			pr_scope = $<scope>2.scope;
		}
	  ')'
		{
			$$ = parse_params ($3);
		}
	| '(' ')'
		{
			$$ = parse_params (0);
		}
	| '(' ELIPSIS ')'
		{
			$$ = parse_params ((def_t*)1);
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
				error (0, "parameter redeclared: %s", $3->name);
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
	| '=' expr
		{
			if (pr_scope) {
				expr_t *e = new_expr ();
				e->type = ex_def;
				e->e.def = current_def;
				append_expr (local_expr, binary_expr ('=', e, $2));
			} else {
				if ($2->type >= ex_string)
					current_def = PR_ReuseConstant ($2,  current_def);
				else
					error ($2, "non-constant expression used for initializer");
			}
		}
	| '=' '#' const
		{
			if (current_type->type != ev_func) {
				error (0, "%s is not a function", current_def->name);
			} else {
				if ($3->type != ex_integer && $3->type != ex_float) {
					error (0, "invalid constant for = #");
				} else {
					function_t	*f;

					f = new_function ();
					f->builtin = $3->type == ex_integer
								 ? $3->e.integer_val : (int)$3->e.float_val;
					f->def = current_def;
					build_function (f);
					finish_function (f);
				}
			}
		}
	| '=' begin_function statement_block end_function
		{
			build_function ($2);
			emit_function ($2, $3);
			finish_function ($2);
		}
	| '=' '[' const ','
		{
			$<def>$ = current_def;
		}
	  def_name
	  	{
			current_def = $<def>5;
		}
	  ']'
		{
			if ($3->type == ex_integer)
				convert_int ($3);
			if ($3->type != ex_float)
				error ($3, "invalid type for frame number");
			if ($6->type->type != ev_func)
				error ($3, "invalid type for think");
			$<expr>$ = new_expr ();
		}
	  begin_function statement_block end_function
		{
			expr_t *e = $<expr>9;
			build_function ($10);
			e->type = ex_expr;
			e->e.expr.op = 's';
			e->e.expr.e1 = $3;
			e->e.expr.e2 = new_expr ();
			e->e.expr.e2->type = ex_def;
			e->e.expr.e2->e.def = $6;
			e->next = $11;

			emit_function ($10, e);
			finish_function ($10);
		}
	;

begin_function
	: /*empty*/
		{
			$$ = current_func = new_function ();
			$$->def = current_def;
			$$->code = numstatements;
			if (options.debug) {
				pr_lineno_t *lineno = new_lineno ();
				$$->aux = new_auxfunction ();
				$$->aux->source_line = pr_source_line;
				$$->aux->line_info = lineno - linenos;
				$$->aux->local_defs = num_locals;

				lineno->fa.func = $$->aux - auxfunctions;
			}
			pr_scope = current_def;
			build_scope ($$, current_def);
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
			//printf("statements: /* empty */\n");
			$$ = new_block_expr ();
		}
	| statements statement
		{
			//printf("statements: statements statement\n");
			$$ = append_expr ($1, $2);
		}
	;

statement
	: ';' { $$ = 0; }
	| statement_block { $$ = $1; }
	| RETURN expr ';'
		{
			$$ = return_expr (current_func, $2);
		}
	| RETURN ';'
		{
			$$ = return_expr (current_func, 0);
		}
	| WHILE '(' expr ')' statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *l2 = new_label_expr ();
			expr_t *e;

			$$ = new_block_expr ();

			e = new_binary_expr ('n', test_expr ($3, 1), l2);
			e->line = $3->line;
			e->file = $3->file;
			append_expr ($$, e);
			append_expr ($$, l1);
			append_expr ($$, $5);
			e = new_binary_expr ('i', test_expr ($3, 1), l1);
			e->line = $3->line;
			e->file = $3->file;
			append_expr ($$, e);
			append_expr ($$, l2);
		}
	| DO statement WHILE '(' expr ')' ';'
		{
			expr_t *l1 = new_label_expr ();

			$$ = new_block_expr ();

			append_expr ($$, l1);
			append_expr ($$, $2);
			append_expr ($$, new_binary_expr ('i', test_expr ($5, 1), l1));
		}
	| LOCAL type
		{
			current_type = $2;
			local_expr = new_block_expr ();
		}
	  def_list ';'
		{
			$$ = local_expr;
			local_expr = 0;
		}
	| IF '(' expr ')' statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *e;

			$$ = new_block_expr ();

			e = new_binary_expr ('n', test_expr ($3, 1), l1);
			e->line = $3->line;
			e->file = $3->file;
			append_expr ($$, e);
			append_expr ($$, $5);
			append_expr ($$, l1);
		}
	| IF '(' expr ')' statement ELSE statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *l2 = new_label_expr ();
			expr_t *e;

			$$ = new_block_expr ();

			e = new_binary_expr ('n', test_expr ($3, 1), l1);
			e->line = $3->line;
			e->file = $3->file;
			append_expr ($$, e);

			append_expr ($$, $5);

			e = new_unary_expr ('g', l2);
			append_expr ($$, e);

			append_expr ($$, l1);
			append_expr ($$, $7);
			append_expr ($$, l2);
		}
	| FOR '(' opt_expr ';' opt_expr ';' opt_expr ')' statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *l2 = new_label_expr ();

			$$ = new_block_expr ();

			append_expr ($$, $3);
			if ($5)
				append_expr ($$, new_binary_expr ('n', test_expr ($5, 1), l2));
			append_expr ($$, l1);
			append_expr ($$, $9);
			append_expr ($$, $7);
			if ($5)
				append_expr ($$, new_binary_expr ('i', test_expr ($5, 1), l1));
			append_expr ($$, l2);
		}
	| expr ';'
		{
			$$ = $1;
		}
	;

opt_expr
	: expr
	| /* empty */
		{
			$$ = 0;
		}

expr
	: expr '=' expr				{ $$ = binary_expr ('=', $1, $3); }
	| expr ASX expr				{ $$ = asx_expr ($2, $1, $3); }
	| expr '?' expr ':' expr 	{ $$ = conditional_expr ($1, $3, $5); }
	| expr AND expr				{ $$ = binary_expr (AND, $1, $3); }
	| expr OR expr				{ $$ = binary_expr (OR,  $1, $3); }
	| expr EQ expr				{ $$ = binary_expr (EQ,  $1, $3); }
	| expr NE expr				{ $$ = binary_expr (NE,  $1, $3); }
	| expr LE expr				{ $$ = binary_expr (LE,  $1, $3); }
	| expr GE expr				{ $$ = binary_expr (GE,  $1, $3); }
	| expr LT expr				{ $$ = binary_expr (LT,  $1, $3); }
	| expr GT expr				{ $$ = binary_expr (GT,  $1, $3); }
	| expr SHL expr				{ $$ = binary_expr (SHL, $1, $3); }
	| expr SHR expr				{ $$ = binary_expr (SHR, $1, $3); }
	| expr '+' expr				{ $$ = binary_expr ('+', $1, $3); }
	| expr '-' expr				{ $$ = binary_expr ('-', $1, $3); }
	| expr '*' expr				{ $$ = binary_expr ('*', $1, $3); }
	| expr '/' expr				{ $$ = binary_expr ('/', $1, $3); }
	| expr '&' expr				{ $$ = binary_expr ('&', $1, $3); }
	| expr '|' expr				{ $$ = binary_expr ('|', $1, $3); }
	| expr '^' expr				{ $$ = binary_expr ('^', $1, $3); }
	| expr '%' expr				{ $$ = binary_expr ('%', $1, $3); }
	| expr '(' arg_list ')'		{ $$ = function_expr ($1, $3); }
	| expr '(' ')'				{ $$ = function_expr ($1, 0); }
	| expr '.' expr				{ $$ = binary_expr ('.', $1, $3); }
	| '+' expr %prec UNARY		{ $$ = $2; }
	| '-' expr %prec UNARY		{ $$ = unary_expr ('-', $2); }
	| '!' expr %prec UNARY		{ $$ = unary_expr ('!', $2); }
	| '~' expr %prec UNARY		{ $$ = unary_expr ('~', $2); }
	| INCOP expr				{ $$ = incop_expr ($1, $2, 0); }
	| expr INCOP				{ $$ = incop_expr ($2, $1, 1); }
	| NAME
		{
			$$ = new_expr ();
			$$->type = ex_def;
			$$->e.def = PR_GetDef (NULL, $1, pr_scope, false);
			if (!$$->e.def) {
				error (0, "Undeclared variable \"%s\".", $1);
				$$->e.def = &def_float;
			}
		}
	| const						{ $$ = $1; }
	| '(' expr ')'				{ $$ = $2; $$->paren = 1; }
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
	| STRING_VAL
		{
			$$ = new_expr ();
			$$->type = ex_string;
			$$->e.string_val = $1;
		}
	| VECTOR_VAL
		{
			$$ = new_expr ();
			$$->type = ex_vector;
			memcpy ($$->e.vector_val, $1, sizeof ($$->e.vector_val));
		}
	| QUATERNION_VAL
		{
			$$ = new_expr ();
			$$->type = ex_quaternion;
			memcpy ($$->e.quaternion_val, $1, sizeof ($$->e.quaternion_val));
		}
	| INT_VAL
		{
			$$ = new_expr ();
			$$->type = ex_integer;
			$$->e.integer_val = $1;
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
			error (0, "too many params");
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

void
build_scope (function_t *f, def_t *func)
{
	int i;
	def_t *def;
	type_t *ftype = func->type;

	for (i = 0; i < ftype->num_parms; i++) {
		def = PR_GetDef (ftype->parm_types[i], pr_parm_names[i], func, &func->num_locals);
		f->parm_ofs[i] = def->ofs;
		if (i > 0 && f->parm_ofs[i] < f->parm_ofs[i - 1])
			Error ("bad parm order");
	}
}

function_t *
new_function (void)
{
	function_t	*f;

	f = calloc (1, sizeof (function_t));
	f->next = pr_functions;
	pr_functions = f;

	return f;
}

void
build_function (function_t *f)
{
	f->def->initialized = 1;
	G_FUNCTION (f->def->ofs) = numfunctions;
}

void
finish_function (function_t *f)
{
	dfunction_t *df;
	int i;

	// fill in the dfunction
	df = &functions[numfunctions];
	numfunctions++;
	f->dfunc = df;

	if (f->builtin)
		df->first_statement = -f->builtin;
	else
		df->first_statement = f->code;

	df->s_name = ReuseString (f->def->name);
	df->s_file = s_file;
	df->numparms = f->def->type->num_parms;
	df->locals = f->def->num_locals;
	df->parm_start = 0;
	for (i = 0; i < df->numparms; i++)
		df->parm_size[i] = type_size[f->def->type->parm_types[i]->type];

	if (f->aux) {
		def_t *def;
		f->aux->function = df - functions;
		for (def = f->def->scope_next; def; def = def->scope_next) {
			if (def->name) {
				ddef_t *d = new_local ();
				d->type = def->type->type;
				d->ofs = def->ofs;
				d->s_name = ReuseString (def->name);

				f->aux->num_locals++;
			}
		}
	}
}

void
emit_function (function_t *f, expr_t *e)
{
	//PR_PrintType (f->def->type);
	//printf (" %s =\n", f->def->name);

	if (f->aux)
		lineno_base = f->aux->source_line;

	pr_scope = f->def;
	while (e) {
		//print_expr (e);
		//puts("");

		emit_expr (e);
		e = e->next;
	}
	emit_statement (pr_source_line, op_done, 0, 0, 0);
	PR_FlushScope (pr_scope);
	pr_scope = 0;
	PR_ResetTempDefs ();

	//puts ("");
}
