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


#include "cmdlib.h"
#include <stdio.h>
#include <setjmp.h>

#include "QF/pr_comp.h"

/*
	TODO:
	o	"stopped at 10 errors"
	o	other pointer types for models and clients?
	o	compact string heap?
	o	allways initialize all variables to something safe
	o	the def->type->type arrangement is really silly.
	o	return type checking
	o	parm count type checking
	o	immediate overflow checking
	o	pass the first two parms in call->b and call->c
*/

/*

	comments
	--------
	// comments discard text until the end of line
	/ *  * / comments discard all enclosed text (spaced out on this line
	because this documentation is in a regular C comment block, and typing
	them in normally causes a parse error)

	code structure
	--------------
	A definition is:
		<type> <name> [ = <immediate>] {, <name> [ = <immediate>] };


	types
	-----
	simple types: void, float, vector, string, or entity
		float		width, height;
		string		name;
		entity		self, other;

	vector types:
		vector		org;	// also creates org_x, org_y, and org_z float defs
		
		
	A function type is specified as: 	simpletype ( type name {,type name} )
	The names are ignored except when the function is initialized.	
		void()		think;
		entity()	FindTarget;
		void(vector destination, float speed, void() callback)	SUB_CalcMove;
		void(...)	dprint;		// variable argument builtin

	A field type is specified as:  .type
		.vector		origin;
		.string		netname;
		.void()		think, touch, use;
		

	names
	-----
	Names are a maximum of 64 characters, must begin with A-Z,a-z, or _, and
	can continue with those characters or 0-9.

	There are two levels of scoping: global, and function.  The parameter list
	of a function and any vars declared inside a function with the "local"
	statement are only visible within that function, 


	immediates
	----------
	Float immediates must begin with 0-9 or minus sign.  .5 is illegal.
		
	A parsing ambiguity is present with negative constants. "a-5" will be
	parsed as "a", then "-5", causing an error.  Seperate the - from the
	digits with a space "a - 5" to get the proper behavior.
		12
		1.6
		0.5
		-100

	Vector immediates are three float immediates enclosed in single quotes.
		'0 0 0'
		'20.5 -10 0.00001'
		
	String immediates are characters enclosed in double quotes.  The string
	cannot contain explicit newlines, but the escape character \n can embed
	one.  The \" escape can be used to include a quote in the string.
		"maps/jrwiz1.bsp"
		"sound/nin/pain.wav"
		"ouch!\n"

	Code immediates are statements enclosed in {} braces.
	statement:
		{ <multiple statements> }
		<expression>;
		local <type> <name> [ = <immediate>] {, <name> [ = <immediate>] };
		return <expression>;
		if ( <expression> ) <statement> [ else <statement> ];
		while ( <expression> ) <statement>;
		do <statement> while ( <expression> );
		<function name> ( <function parms> );
		
	expression:
		combiations of names and these operators with standard C precedence:
		"&&", "||", "<=", ">=","==", "!=", "!", "*", "/", "-", "+", "=", ".",
		"<", ">", "&", "|"
		Parenthesis can be used to alter order of operation.
		The & and | operations perform integral bit ops on floats
		
	A built in function immediate is a number sign followed by an integer.
		#1
		#12


	compilation
	-----------
	Source files are processed sequentially without dumping any state, so if a
	defs file is the first one processed, the definitions will be available to
	all other files.

	The language is strongly typed and there are no casts.

	Anything that is initialized is assumed to be constant, and will have
	immediates folded into it.  If you change the value, your program will
	malfunction.  All uninitialized globals will be saved to savegame files.

	Functions cannot have more than eight parameters.

	Error recovery during compilation is minimal.  It will skip to the next
	global definition, so you will never see more than one error at a time in
	a given function.  All compilation aborts after ten error messages.

	Names can be defined multiple times until they are defined with an
	initialization, allowing functions to be prototyped before their
	definition.

	void()	MyFunction;			// the prototype

	void()	MyFunction =		// the initialization
	{
		dprint ("we're here\n");
	};


	entities and fields
	-------------------


	execution
	---------
	Code execution is initiated by C code in quake from two main places:  the
	timed think routines for periodic control, and the touch function when two
	objects impact each other.

	There are three global variables that are set before beginning code
	execution:
		entity	world;		// the server's world object, which holds all
							// global state for the server, like the
							// deathmatch flags and the body ques.
		entity	self;		// the entity the function is executing for
		entity	other;		// the other object in an impact, not used for
							// thinks
		float	time;		// the current game time.  Note that because the
							// entities in the world are simulated
							// sequentially, time is NOT strictly increasing.
							// An impact late in one entity's time slice may
							// set time higher than the think function of the
							// next entity.  The difference is limited to 0.1
							// seconds.
	Execution is also caused by a few uncommon events, like the addition of a
	new client to an existing server.
		
	There is a runnaway counter that stops a program if 100000 statements are
	executed, assuming it is in an infinite loop.

	It is acceptable to change the system set global variables.  This is
	usually done to pose as another entity by changing self and calling a
	function.

	The interpretation is fairly efficient, but it is still over an order of
	magnitude slower than compiled C code.  All time consuming operations
	should be made into built in functions.

	A profile counter is kept for each function, and incremented for each
	interpreted instruction inside that function.  The "profile" console
	command in Quake will dump out the top 10 functions, then clear all the
	counters.  The "profile all" command will dump sorted stats for every
	function that has been executed.


	afunc ( 4, bfunc(1,2,3));
	will fail because there is a shared parameter marshaling area, which will
	cause the 1 from bfunc to overwrite the 4 allready placed in parm0.  When
	a function is called, it copies the parms from the globals into it's
	privately scoped variables, so there is no collision when calling another
	function.

	total = factorial(3) + factorial(4);
	Will fail because the return value from functions is held in a single
	global area.  If this really gets on your nerves, tell me and I can work
	around it at a slight performance and space penalty by allocating a new
	register for the function call and copying it out.


	built in functions
	------------------
	void(string text)	dprint;
	Prints the string to the server console.

	void(entity client, string text)	cprint;
	Prints a message to a specific client.

	void(string text)	bprint;
	Broadcast prints a message to all clients on the current server.

	entity()	spawn;
	Returns a totally empty entity.  You can manually set everything up, or
	just set the origin and call one of the existing entity setup functions.

	entity(entity start, .string field, string match) find;
	Searches the server entity list beginning at start, looking for an entity
	that has entity.field = match.  To start at the beginning of the list,
	pass world.  World is returned when the end of the list is reached.

	<FIXME: define all the other functions...>


	gotchas
	-------
	o	The && and || operators DO NOT EARLY OUT like C!
	o	Don't confuse single quoted vectors with double quoted strings
	o	The function declaration syntax takes a little getting used to.
	o	Don't forget the ; after the trailing brace of a function
		initialization.
	o	Don't forget the "local" before defining local variables.
	o	There are no ++ / -- operators, or operate/assign operators.
*/

//=============================================================================

// offsets are allways multiplied by 4 before using
typedef int	gofs_t;				// offset in global data block
typedef struct function_s function_t;

#define	MAX_PARMS	8

typedef struct type_s
{
	etype_t			type;
	struct def_s	*def;		// a def that points to this type
	struct type_s	*next;
// function types are more complex
	struct type_s	*aux_type;	// return type or field type
	int				num_parms;	// -1 = variable args
	struct type_s	*parm_types[MAX_PARMS];	// only [num_parms] allocated
} type_t;

typedef struct statref_s {
	struct statref_s *next;
	dstatement_t	*statement;
	int				field;		// a, b, c (0, 1, 2)
} statref_t;

typedef struct def_s {
	type_t			*type;
	const char		*name;
	int				num_locals;
	gofs_t			ofs;
	int				initialized;// 1 when a declaration included "= immediate"
	statref_t		*refs;		// for relocations

	struct def_s	*def_next;		// for writing out the global defs list
	struct def_s	*next;			// general purpose linking
	struct def_s	*scope_next;	// to facilitate hash table removal
	struct def_s	*scope;			// function the var was defined in, or NULL
} def_t;

//============================================================================

// pr_loc.h -- program local defs

#define	MAX_ERRORS		10

#define	MAX_NAME		64		// chars long

#define	MAX_REGS		65536

//=============================================================================

typedef union eval_s
{
	string_t			string;
	float				_float;
	float				vector[3];
	func_t				function;
	int					_int;
	union eval_s		*ptr;
} eval_t;	

extern	int		type_size[8];
extern	def_t	*def_for_type[8];

extern	type_t	type_void;
extern	type_t	type_string;
extern	type_t	type_float;
extern	type_t	type_vector;
extern	type_t	type_entity;
extern	type_t	type_field;
extern	type_t	type_function;
extern	type_t	type_pointer;
extern	type_t	type_floatfield;

extern	def_t	def_void;
extern	def_t	def_string;
extern	def_t	def_float;
extern	def_t	def_vector;
extern	def_t	def_entity;
extern	def_t	def_field;
extern	def_t	def_function;
extern	def_t	def_pointer;

struct function_s
{
	struct function_s	*next;
	dfunction_t			*dfunc;
	int					builtin;	// if non 0, call an internal function
	int					code;		// first statement
	const char			*file;		// source file with definition
	int					file_line;
	struct def_s		*def;
	int					parm_ofs[MAX_PARMS];	// allways contiguous, right?
};

function_t *pr_functions;

//
// output generated by prog parsing
//
typedef struct
{
	char		*memory;
	int			max_memory;
	int			current_memory;
	type_t		*types;
	
	def_t		def_head;		// unused head of linked list
	def_t		*def_tail;		// add new defs after this and move it
	def_t		*search;		// search chain through defs

	int			size_fields;
} pr_info_t;

extern	pr_info_t	pr;

typedef struct
{
	const char	*name;
	const char	*opname;
	pr_opcode_e opcode;
	int			priority;
	qboolean	right_associative;
	def_t		*type_a, *type_b, *type_c;
	int			min_version;
} opcode_t;

extern opcode_t *op_done;
extern opcode_t *op_return;
extern opcode_t *op_if;
extern opcode_t *op_ifnot;
extern opcode_t *op_state;
extern opcode_t *op_goto;

def_t *PR_Statement (opcode_t *op, def_t *var_a, def_t *var_b);
opcode_t *PR_Opcode_Find (const char *name, int priority,
						  def_t *var_a, def_t *var_b, def_t *var_c);
opcode_t *PR_Opcode (short opcode);
void PR_Opcode_Init (void);

//============================================================================


extern	qboolean	pr_dumpasm;

extern	def_t		*pr_global_defs[MAX_REGS];	// to find def for a global
												// variable

typedef enum {
	tt_eof,			// end of file reached
	tt_name, 		// an alphanumeric name token
	tt_punct, 		// code punctuation
	tt_immediate,	// string, float, vector
} token_type_t;

extern	char		pr_token[2048];
extern	token_type_t	pr_token_type;
extern	type_t		*pr_immediate_type;
extern	eval_t		pr_immediate;

void PR_PrintStatement (dstatement_t *s);

void PR_Lex (void);
// reads the next token into pr_token and classifies its type

type_t *PR_ParseType (void);
char *PR_ParseName (void);
def_t *PR_ParseImmediate (def_t *def);

qboolean PR_Check (token_type_t type, const char *string);
void PR_Expect (token_type_t type, const char *string);
void PR_ParseError (const char *error, ...);


extern	jmp_buf		pr_parse_abort;		// longjump with this on parse error
extern	int			pr_source_line;
extern	char		*pr_file_p;

void *PR_Malloc (int size);


#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	RESERVED_OFS	28


extern	def_t	*pr_scope;
extern	int		pr_error_count;

void PR_NewLine (void);
def_t *PR_GetDef (type_t *type, const char *name, def_t *scope,
				  int *allocate);
def_t *PR_NewDef (type_t *type, const char *name, def_t *scope);
int PR_NewLocation (type_t *type);
void PR_FreeLocation (def_t *def);
def_t *PR_GetTempDef (type_t *type, def_t *scope);
void PR_FreeTempDefs ();
void PR_ResetTempDefs ();
void PR_FlushScope (def_t *scope);

void PR_PrintDefs (void);
void PR_PrintFunction (def_t *def);
void PR_SkipToSemicolon (void);

extern	char		pr_parm_names[MAX_PARMS][MAX_NAME];
extern	qboolean	pr_trace;

#define	G_FLOAT(o) (pr_globals[o])
#define	G_INT(o) (*(int *)&pr_globals[o])
#define	G_VECTOR(o) (&pr_globals[o])
#define	G_STRING(o) (strings + *(string_t *)&pr_globals[o])
#define	G_FUNCTION(o) (*(func_t *)&pr_globals[o])

char *PR_ValueString (etype_t type, void *val);

void PR_ClearGrabMacros (void);

qboolean	PR_CompileFile (char *string, const char *filename);

extern	qboolean	pr_dumpasm;

extern	string_t	s_file;			// filename for function definition

extern	def_t	def_ret, def_parms[MAX_PARMS];

//=============================================================================

#define	MAX_STRINGS		500000
#define	MAX_GLOBALS		65536
#define	MAX_FIELDS		1024
#define	MAX_STATEMENTS	131072
#define	MAX_FUNCTIONS	8192

#define	MAX_SOUNDS		1024
#define	MAX_MODELS		1024
#define	MAX_FILES		1024
#define	MAX_DATA_PATH	64

extern	char	strings[MAX_STRINGS];
extern	int		strofs;

extern	dstatement_t	statements[MAX_STATEMENTS];
extern	int			numstatements;
extern	int			statement_linenums[MAX_STATEMENTS];

extern	dfunction_t	functions[MAX_FUNCTIONS];
extern	int			numfunctions;

extern	float		pr_globals[MAX_REGS];
extern	int			numpr_globals;

extern	char	pr_immediate_string[2048];

extern	char		precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
extern	int			precache_sounds_block[MAX_SOUNDS];
extern	int			numsounds;

extern	char		precache_models[MAX_MODELS][MAX_DATA_PATH];
extern	int			precache_models_block[MAX_SOUNDS];
extern	int			nummodels;

extern	char		precache_files[MAX_FILES][MAX_DATA_PATH];
extern	int			precache_files_block[MAX_SOUNDS];
extern	int			numfiles;

int	CopyString (const char *str);
int	ReuseString (const char *str);

typedef struct {
	int		cow;		// copy on write for constants
	int		version;	// maximum progs version to support (eg, 6 for id)
} options_t;

extern options_t options;
