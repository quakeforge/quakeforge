/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qfcc.h"


pr_info_t	pr;
def_t		*pr_global_defs[MAX_REGS];	// to find def for a global variable
int 		pr_edict_size;

//========================================

def_t		*pr_scope;					// the function being parsed, or NULL
qboolean	pr_dumpasm;
string_t	s_file;						// filename for function definition

int 		locals_end;					// for tracking local variables vs temps

jmp_buf     pr_parse_abort;				// longjump with this on parse error

void PR_ParseDefs (void);

//========================================


opcode_t    pr_opcodes[] = {
	{"<DONE>", "DONE", -1, false, &def_entity, &def_field, &def_void},

	{"*", "MUL_F", 2, false, &def_float, &def_float, &def_float},
	{"*", "MUL_V", 2, false, &def_vector, &def_vector, &def_float},
	{"*", "MUL_FV", 2, false, &def_float, &def_vector, &def_vector},
	{"*", "MUL_VF", 2, false, &def_vector, &def_float, &def_vector},

	{"/", "DIV", 2, false, &def_float, &def_float, &def_float},

	{"+", "ADD_F", 3, false, &def_float, &def_float, &def_float},
	{"+", "ADD_V", 3, false, &def_vector, &def_vector, &def_vector},

	{"-", "SUB_F", 3, false, &def_float, &def_float, &def_float},
	{"-", "SUB_V", 3, false, &def_vector, &def_vector, &def_vector},

	{"==", "EQ_F", 4, false, &def_float, &def_float, &def_float},
	{"==", "EQ_V", 4, false, &def_vector, &def_vector, &def_float},
	{"==", "EQ_S", 4, false, &def_string, &def_string, &def_float},
	{"==", "EQ_E", 4, false, &def_entity, &def_entity, &def_float},
	{"==", "EQ_FNC", 4, false, &def_function, &def_function, &def_float},

	{"!=", "NE_F", 4, false, &def_float, &def_float, &def_float},
	{"!=", "NE_V", 4, false, &def_vector, &def_vector, &def_float},
	{"!=", "NE_S", 4, false, &def_string, &def_string, &def_float},
	{"!=", "NE_E", 4, false, &def_entity, &def_entity, &def_float},
	{"!=", "NE_FNC", 4, false, &def_function, &def_function, &def_float},

	{"<=", "LE", 4, false, &def_float, &def_float, &def_float},
	{">=", "GE", 4, false, &def_float, &def_float, &def_float},
	{"<", "LT", 4, false, &def_float, &def_float, &def_float},
	{">", "GT", 4, false, &def_float, &def_float, &def_float},

	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_float},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_vector},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_string},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_entity},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_field},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_function},

	{".", "ADDRESS", 1, false, &def_entity, &def_field, &def_pointer},

	{"=", "STORE_F", 5, true, &def_float, &def_float, &def_float},
	{"=", "STORE_V", 5, true, &def_vector, &def_vector, &def_vector},
	{"=", "STORE_S", 5, true, &def_string, &def_string, &def_string},
	{"=", "STORE_ENT", 5, true, &def_entity, &def_entity, &def_entity},
	{"=", "STORE_FLD", 5, true, &def_field, &def_field, &def_field},
	{"=", "STORE_FNC", 5, true, &def_function, &def_function, &def_function},

	{"=", "STOREP_F", 5, true, &def_pointer, &def_float, &def_float},
	{"=", "STOREP_V", 5, true, &def_pointer, &def_vector, &def_vector},
	{"=", "STOREP_S", 5, true, &def_pointer, &def_string, &def_string},
	{"=", "STOREP_ENT", 5, true, &def_pointer, &def_entity, &def_entity},
	{"=", "STOREP_FLD", 5, true, &def_pointer, &def_field, &def_field},
	{"=", "STOREP_FNC", 5, true, &def_pointer, &def_function, &def_function},

	{"<RETURN>", "RETURN", -1, false, &def_void, &def_void, &def_void},

	{"!", "NOT_F", -1, false, &def_float, &def_void, &def_float},
	{"!", "NOT_V", -1, false, &def_vector, &def_void, &def_float},
	{"!", "NOT_S", -1, false, &def_vector, &def_void, &def_float},
	{"!", "NOT_ENT", -1, false, &def_entity, &def_void, &def_float},
	{"!", "NOT_FNC", -1, false, &def_function, &def_void, &def_float},

	{"<IF>", "IF", -1, false, &def_float, &def_float, &def_void},
	{"<IFNOT>", "IFNOT", -1, false, &def_float, &def_float, &def_void},

// calls returns REG_RETURN
	{"<CALL0>", "CALL0", -1, false, &def_function, &def_void, &def_void},
	{"<CALL1>", "CALL1", -1, false, &def_function, &def_void, &def_void},
	{"<CALL2>", "CALL2", -1, false, &def_function, &def_void, &def_void},
	{"<CALL3>", "CALL3", -1, false, &def_function, &def_void, &def_void},
	{"<CALL4>", "CALL4", -1, false, &def_function, &def_void, &def_void},
	{"<CALL5>", "CALL5", -1, false, &def_function, &def_void, &def_void},
	{"<CALL6>", "CALL6", -1, false, &def_function, &def_void, &def_void},
	{"<CALL7>", "CALL7", -1, false, &def_function, &def_void, &def_void},
	{"<CALL8>", "CALL8", -1, false, &def_function, &def_void, &def_void},

	{"<STATE>", "STATE", -1, false, &def_float, &def_float, &def_void},

	{"<GOTO>", "GOTO", -1, false, &def_float, &def_void, &def_void},

	{"&&", "AND", 6, false, &def_float, &def_float, &def_float},
	{"||", "OR", 6, false, &def_float, &def_float, &def_float},

	{"&", "BITAND", 2, false, &def_float, &def_float, &def_float},
	{"|", "BITOR", 2, false, &def_float, &def_float, &def_float},

	{"+", "ADD_S", 3, false, &def_string, &def_string, &def_string},

	{NULL}
};

#define	TOP_PRIORITY	6
#define	NOT_PRIORITY	4

def_t	junkdef;

def_t *PR_Expression (int priority);

//===========================================================================


/*
	PR_Statement

	Emits a primitive statement, returning the var it places it's value in
*/
def_t *
PR_Statement (opcode_t * op, def_t * var_a, def_t * var_b)
{
	dstatement_t	*statement;
	def_t			*var_c;

	statement = &statements[numstatements];
	numstatements++;

	statement_linenums[statement - statements] = pr_source_line;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (op->type_c == &def_void || op->right_associative) {
		// ifs, gotos, and assignments don't need vars allocated
		var_c = NULL;
		statement->c = 0;
	} else {	// allocate result space
		var_c = malloc (sizeof (def_t));
		memset (var_c, 0, sizeof (def_t));
		var_c->ofs = numpr_globals;
		var_c->type = op->type_c->type;

		statement->c = numpr_globals;
		numpr_globals += type_size[op->type_c->type->type];
	}

	if (op->right_associative)
		return var_a;

	return var_c;
}


void
PrecacheSound (def_t *e, int ch)
{
	char	*n;
	int 	i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numsounds; i++) {
		if (!strcmp (n, precache_sounds[i])) {
			return;
		}
	}

	if (numsounds == MAX_SOUNDS)
		Error ("PrecacheSound: numsounds == MAX_SOUNDS");

	strcpy (precache_sounds[i], n);
	if (ch >= '1' && ch <= '9')
		precache_sounds_block[i] = ch - '0';
	else
		precache_sounds_block[i] = 1;

	numsounds++;
}

void
PrecacheModel (def_t *e, int ch)
{
	char	*n;
	int 	i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < nummodels; i++) {
		if (!strcmp (n, precache_models[i])) {
			return;
		}
	}

	if (numsounds == MAX_SOUNDS)
		Error ("PrecacheModels: numsounds == MAX_SOUNDS");

	strcpy (precache_models[i], n);
	if (ch >= '1' && ch <= '9')
		precache_models_block[i] = ch - '0';
	else
		precache_models_block[i] = 1;

	nummodels++;
}

void
PrecacheFile (def_t *e, int ch)
{
	char	*n;
	int 	i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numfiles; i++) {
		if (!strcmp (n, precache_files[i])) {
			return;
		}
	}

	if (numfiles == MAX_FILES)
		Error ("PrecacheFile: numfiles == MAX_FILES");

	strcpy (precache_files[i], n);
	if (ch >= '1' && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;

	numfiles++;
}

/*
	PR_ParseFunctionCall
*/
def_t *
PR_ParseFunctionCall (def_t *func)
{
	def_t	*e;
	int 	arg;
	type_t	*t;

	t = func->type;

	if (t->type != ev_func)
		PR_ParseError ("not a function");

	// copy the arguments to the global parameter variables
	arg = 0;
	if (!PR_Check (tt_punct, ")")) {
		do {
			if (t->num_parms != -1 && arg >= t->num_parms)
				PR_ParseError ("too many parameters");

			e = PR_Expression (TOP_PRIORITY);

			if (arg == 0 && func->name) {
				// save information for model and sound caching
				if (!strncmp (func->name, "precache_sound", 14))
					PrecacheSound (e, func->name[14]);
				else if (!strncmp (func->name, "precache_model", 14))
					PrecacheModel (e, func->name[14]);
				else if (!strncmp (func->name, "precache_file", 13))
					PrecacheFile (e, func->name[13]);
			}

			if (t->num_parms != -1 && (e->type != t->parm_types[arg]))
				PR_ParseError ("type mismatch on parm %i", arg);

			// a vector copy will copy everything
			def_parms[arg].type = t->parm_types[arg];
			PR_Statement (&pr_opcodes[OP_STORE_V], e, &def_parms[arg]);
			arg++;
		} while (PR_Check (tt_punct, ","));

		if (t->num_parms != -1 && arg != t->num_parms)
			PR_ParseError ("too few parameters");
		PR_Expect (tt_punct, ")");
	}

	if (arg > 8)
		PR_ParseError ("More than eight parameters");

	PR_Statement (&pr_opcodes[OP_CALL0 + arg], func, 0);

	def_ret.type = t->aux_type;
	return &def_ret;
}

/*
	PR_ParseValue

	Return the global offset for the current token
*/
def_t *
PR_ParseValue (void)
{
	def_t	*d;
	char	*name;

	// if the token is an immediate, allocate a constant for it
	if (pr_token_type == tt_immediate)
		return PR_ParseImmediate (0);

	name = PR_ParseName ();

	// look through the defs
	if (!(d = PR_GetDef (NULL, name, pr_scope, false)))
		PR_ParseError ("Unknown value \"%s\"", name);

	return d;
}


/*
	PR_Term
*/
def_t *
PR_Term (void)
{
	def_t		*e;
	etype_t 	t;

	if (PR_Check (tt_punct, "!")) {
		e = PR_Expression (NOT_PRIORITY);
		t = e->type->type;
		switch (t) {
			case ev_float:
				return PR_Statement (&pr_opcodes[OP_NOT_F], e, 0);
			case ev_string:
				return PR_Statement (&pr_opcodes[OP_NOT_S], e, 0);
			case ev_entity:
				return PR_Statement (&pr_opcodes[OP_NOT_ENT], e, 0);
			case ev_vector:
				return PR_Statement (&pr_opcodes[OP_NOT_V], e, 0);
			case ev_func:
				return PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0);
			default:
				PR_ParseError ("Type mismatch for !");
				return NULL;
		}
	}

	if (PR_Check (tt_punct, "(")) {
		e = PR_Expression (TOP_PRIORITY);
		PR_Expect (tt_punct, ")");
		return e;
	}

	return PR_ParseValue ();
}


/*
	PR_Expression
*/
def_t *
PR_Expression (int priority)
{
	opcode_t	*op, *oldop;
	def_t		*e, *e2;
	etype_t 	type_a, type_b, type_c;

	if (!priority)
		return PR_Term ();

	e = PR_Expression (priority - 1);

	while (1) {
		if (priority == 1 && PR_Check (tt_punct, "("))
			return PR_ParseFunctionCall (e);

		for (op = pr_opcodes; op->name; op++) {

			if (op->priority != priority)
				continue;

			if (!PR_Check (tt_punct, op->name))
				continue;

			if (op->right_associative) {
				// if last statement is an indirect, change it to an address of
				if ((unsigned) (statements[numstatements - 1].op - OP_LOAD_F) < 6) {
					statements[numstatements - 1].op = OP_ADDRESS;
					def_pointer.type->aux_type = e->type;
					e->type = def_pointer.type;
				}
				e2 = PR_Expression (priority);
			} else {
				e2 = PR_Expression (priority - 1);
			}

			// type check
			type_a = e->type->type;
			type_b = e2->type->type;

			if (op->name[0] == '.') {	// field access gets type from field
				if (e2->type->aux_type)
					type_c = e2->type->aux_type->type;
				else
					type_c = -1;		// not a field
			} else {
				type_c = ev_void;
			}

			oldop = op;
			while (type_a != op->type_a->type->type
				   || type_b != op->type_b->type->type
				   || (type_c != ev_void && type_c != op->type_c->type->type)
				   || strcmp (op->name, oldop->name)) {
				op++;
				if (!op->name)// || strcmp (op->name, oldop->name))
					PR_ParseError ("type mismatch for %s", oldop->name);
			}

			if (type_a == ev_pointer && type_b != e->type->aux_type->type)
				PR_ParseError ("type mismatch for %s", op->name);

			if (op->right_associative)
				e = PR_Statement (op, e2, e);
			else
				e = PR_Statement (op, e, e2);

			if (type_c != ev_void)		// field access gets type from field
				e->type = e2->type->aux_type;

			break;
		}

		if (!op->name)	// next token isn't at this priority level
			break;
	}

	return e;
}


/*
	PR_ParseStatement
*/
void
PR_ParseStatement (void)
{
	def_t			*e;
	dstatement_t	*patch1, *patch2;

	if (PR_Check (tt_punct, "{")) {
		do {
			PR_ParseStatement ();
		} while (!PR_Check (tt_punct, "}"));
		return;
	}

	if (PR_Check (tt_name, "return")) {
		if (PR_Check (tt_punct, ";")) {
			PR_Statement (&pr_opcodes[OP_RETURN], 0, 0);
			return;
		}

		e = PR_Expression (TOP_PRIORITY);

		PR_Expect (tt_punct, ";");
		PR_Statement (&pr_opcodes[OP_RETURN], e, 0);

		return;
	}

	if (PR_Check (tt_name, "while")) {
		PR_Expect (tt_punct, "(");
		patch2 = &statements[numstatements];
		e = PR_Expression (TOP_PRIORITY);
		PR_Expect (tt_punct, ")");
		patch1 = &statements[numstatements];
		PR_Statement (&pr_opcodes[OP_IFNOT], e, 0);
		PR_ParseStatement ();
		junkdef.ofs = patch2 - &statements[numstatements];
		PR_Statement (&pr_opcodes[OP_GOTO], &junkdef, 0);
		patch1->b = &statements[numstatements] - patch1;
		return;
	}

	if (PR_Check (tt_name, "do")) {
		patch1 = &statements[numstatements];
		PR_ParseStatement ();
		PR_Expect (tt_name, "while");
		PR_Expect (tt_punct, "(");
		e = PR_Expression (TOP_PRIORITY);
		PR_Expect (tt_punct, ")");
		PR_Expect (tt_punct, ";");
		junkdef.ofs = patch1 - &statements[numstatements];
		PR_Statement (&pr_opcodes[OP_IF], e, &junkdef);
		return;
	}

	if (PR_Check (tt_name, "local")) {
		PR_ParseDefs ();
		locals_end = numpr_globals;
		return;
	}

	if (PR_Check (tt_name, "if")) {
		PR_Expect (tt_punct, "(");
		e = PR_Expression (TOP_PRIORITY);
		PR_Expect (tt_punct, ")");

		patch1 = &statements[numstatements];
		PR_Statement (&pr_opcodes[OP_IFNOT], e, 0);

		PR_ParseStatement ();

		if (PR_Check (tt_name, "else")) {
			patch2 = &statements[numstatements];
			PR_Statement (&pr_opcodes[OP_GOTO], 0, 0);
			patch1->b = &statements[numstatements] - patch1;
			PR_ParseStatement ();
			patch2->a = &statements[numstatements] - patch2;
		} else {
			patch1->b = &statements[numstatements] - patch1;
		}

		return;
	}

	PR_Expression (TOP_PRIORITY);
	PR_Expect (tt_punct, ";");
}


/*
	PR_ParseState

	States are special functions made for convenience.  They automatically
	set frame, nextthink (implicitly), and think (allowing forward definitions).

		void() name = [framenum, nextthink] {code}

	expands to:

		function void name ()
		{
			self.frame=framenum;
			self.nextthink = time + 0.1;
			self.think = nextthink
			<code>
		};

	Weird, huh? :)
*/
void
PR_ParseState (void)
{
	char	*name;
	def_t	*s1, *def;

	if (pr_token_type != tt_immediate || pr_immediate_type != &type_float)
		PR_ParseError ("state frame must be a number");

	s1 = PR_ParseImmediate (0);

	PR_Expect (tt_punct, ",");

	name = PR_ParseName ();
	def = PR_GetDef (&type_function, name, 0, true);

	PR_Expect (tt_punct, "]");

	PR_Statement (&pr_opcodes[OP_STATE], s1, def);
}

/*
	PR_ParseImmediateStatements

	Parse a function body
*/
function_t *
PR_ParseImmediateStatements (type_t *type)
{
	int 		i;
	function_t	*f;
	def_t		*defs[MAX_PARMS];

	f = malloc (sizeof (function_t));

	// check for builtin function definition #1, #2, etc
	if (PR_Check (tt_punct, "#")) {
		if (pr_token_type != tt_immediate
			|| pr_immediate_type != &type_float
			|| pr_immediate._float != (int) pr_immediate._float) {
			PR_ParseError ("Bad builtin immediate");
		}

		f->builtin = (int) pr_immediate._float;
		PR_Lex ();
		return f;
	}

	f->builtin = 0;

	// define the parms
	for (i = 0; i < type->num_parms; i++) {
		defs[i] = PR_GetDef (type->parm_types[i], pr_parm_names[i], pr_scope, true);
		f->parm_ofs[i] = defs[i]->ofs;
		if (i > 0 && f->parm_ofs[i] < f->parm_ofs[i - 1])
			Error ("bad parm order");
	}

	f->code = numstatements;

	// check for a state opcode
	if (PR_Check (tt_punct, "["))
		PR_ParseState ();

	// parse regular statements
	PR_Expect (tt_punct, "{");

	while (!PR_Check (tt_punct, "}"))
		PR_ParseStatement ();

	// emit an end of statements opcode
	PR_Statement (pr_opcodes, 0, 0);

	return f;
}

/*
	PR_ParseDefs

	Called at the outer layer and when a local statement is hit
*/
void
PR_ParseDefs (void)
{
	char		*name;
	type_t		*type;
	def_t		*def;
	function_t	*f;
	dfunction_t *df;
	int 		i;
	int 		locals_start;

	type = PR_ParseType ();

	if (pr_scope && (type->type == ev_field || type->type == ev_func))
		PR_ParseError ("Fields and functions must be global");

	do {
		name = PR_ParseName ();

		def = PR_GetDef (type, name, pr_scope, true);

		// check for an initialization
		if (PR_Check (tt_punct, "=")) {
			if (def->initialized)
				PR_ParseError ("%s redeclared", name);

			if (type->type == ev_func) {
				locals_start = locals_end = numpr_globals;
				pr_scope = def;
				f = PR_ParseImmediateStatements (type);
				PR_FlushScope (pr_scope);
				pr_scope = NULL;
				def->initialized = 1;
				G_FUNCTION (def->ofs) = numfunctions;
				f->def = def;

//				if (pr_dumpasm)
//					PR_PrintFunction (def);

				// fill in the dfunction
				df = &functions[numfunctions];
				numfunctions++;

				if (f->builtin)
					df->first_statement = -f->builtin;
				else
					df->first_statement = f->code;

				df->s_name = ReuseString (f->def->name);
				df->s_file = s_file;
				df->numparms = f->def->type->num_parms;
				df->locals = locals_end - locals_start;
				df->parm_start = locals_start;
				for (i = 0; i < df->numparms; i++)
					df->parm_size[i] = type_size[f->def->type->parm_types[i]->type];

				continue;
			} else if (pr_immediate_type != type) {
				PR_ParseError ("wrong immediate type for %s", name);
			}

			def = PR_ParseImmediate (def);
		}

	} while (PR_Check (tt_punct, ","));

	PR_Expect (tt_punct, ";");
}

/*
	PR_CompileFile

	Compile the null-terminated text, adding definitions to the pr structure
*/
qboolean
PR_CompileFile (char *string, char *filename)
{
	if (!pr.memory)
		Error ("PR_CompileFile: Didn't clear");

	PR_ClearGrabMacros ();				// clear the frame macros

	pr_file_p = string;
	s_file = ReuseString (filename);

	pr_source_line = 0;

	PR_NewLine ();

	PR_Lex ();							// read first token

	while (pr_token_type != tt_eof) {
		if (setjmp (pr_parse_abort)) {

			if (++pr_error_count > MAX_ERRORS)
				return false;

			PR_SkipToSemicolon ();
			if (pr_token_type == tt_eof)
				return false;
		}

		pr_scope = NULL;				// outside all functions

		PR_ParseDefs ();
	}

	return (pr_error_count == 0);
}
