%{
%}

%left	OP_OR OP_AND
%right	'='
%left	OP_EQ OP_NE OP_LE OP_GE OP_LT OP_GT
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
	: expr OP_AND expr
	| expr OP_OR expr
	| expr '=' expr
	| expr OP_EQ expr
	| expr OP_NE expr
	| expr OP_LE expr
	| expr OP_GE expr
	| expr OP_LT expr
	| expr OP_GT expr
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
