/* 
	qfcc.c

	QuakeForge Code Compiler (main program)

	Copyright (C) 1996-1997 id Software, Inc.
	Copyright (C) 2001 Jeff Teunissen <deek@dusknet.dhs.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <QF/crc.h>
#include <QF/hash.h>
#include <QF/qendian.h>
#include <QF/sys.h>

#include "qfcc.h"


char        sourcedir[1024];
char        destfile[1024];

float       pr_globals[MAX_REGS];
int         numpr_globals;

char        strings[MAX_STRINGS];
int         strofs;

dstatement_t statements[MAX_STATEMENTS];
int         numstatements;
int         statement_linenums[MAX_STATEMENTS];

dfunction_t functions[MAX_FUNCTIONS];
int         numfunctions;

ddef_t      globals[MAX_GLOBALS];
int         numglobaldefs;

ddef_t      fields[MAX_FIELDS];
int         numfielddefs;

char        precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
int         precache_sounds_block[MAX_SOUNDS];
int         numsounds;

char        precache_models[MAX_MODELS][MAX_DATA_PATH];
int         precache_models_block[MAX_SOUNDS];
int         nummodels;

char        precache_files[MAX_FILES][MAX_DATA_PATH];
int         precache_files_block[MAX_SOUNDS];
int         numfiles;

/*
	WriteFiles

	Generates files.dat, which contains all of the data files actually used by
	the game, to be processed by qfiles
*/
void
WriteFiles (void)
{
	FILE	*f;
	int 	i;
	char	filename[1024];

	sprintf (filename, "%s/files.dat", sourcedir);
	f = fopen (filename, "w");
	if (!f)
		Error ("Couldn't open %s", filename);

	fprintf (f, "%i\n", numsounds);
	for (i = 0; i < numsounds; i++)
		fprintf (f, "%i %s\n", precache_sounds_block[i], precache_sounds[i]);

	fprintf (f, "%i\n", nummodels);
	for (i = 0; i < nummodels; i++)
		fprintf (f, "%i %s\n", precache_models_block[i], precache_models[i]);

	fprintf (f, "%i\n", numfiles);
	for (i = 0; i < numfiles; i++)
		fprintf (f, "%i %s\n", precache_files_block[i], precache_files[i]);

	fclose (f);
}

/*
	CopyString

	Return an offset from the string heap
*/
static hashtab_t *strings_tab;

static const char *
stings_get_key (void *_str, void *unsued)
{
	return (char*)_str;
}

int
CopyString (const char *str)
{
	int 	old;

	if (!strings_tab) {
		strings_tab = Hash_NewTable (16381, stings_get_key, 0, 0);
	}
	old = strofs;
	strcpy (strings + strofs, str);
	strofs += strlen (str) + 1;
	Hash_Add (strings_tab, strings + old);
	return old;
}

int
ReuseString (const char *str)
{
	char *s;

	if (!strings_tab)
		return CopyString (str);
	s = Hash_Find (strings_tab, str);
	if (s)
		return s - strings;
	return CopyString (str);
}

void
PrintStrings (void)
{
	int 	i, l, j;

	for (i = 0; i < strofs; i += l) {
		l = strlen (strings + i) + 1;
		printf ("%5i : ", i);
		for (j = 0; j < l; j++) {
			if (strings[i + j] == '\n') {
				putchar ('\\');
				putchar ('n');
			} else {
				putchar (strings[i + j]);
			}
		}
		printf ("\n");
	}
}

void
PrintFunctions (void)
{
	int 		i, j;
	dfunction_t *d;

	for (i = 0; i < numfunctions; i++) {
		d = &functions[i];
		printf ("%s : %s : %i %i (", strings + d->s_file, strings + d->s_name,
				d->first_statement, d->parm_start);
		for (j = 0; j < d->numparms; j++)
			printf ("%i ", d->parm_size[j]);
		printf (")\n");
	}
}

void
PrintFields (void)
{
	int 	i;
	ddef_t	*d;

	for (i = 0; i < numfielddefs; i++) {
		d = &fields[i];
		printf ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}

void
PrintGlobals (void)
{
	int 	i;
	ddef_t	*d;

	for (i = 0; i < numglobaldefs; i++) {
		d = &globals[i];
		printf ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}


void
InitData (void)
{
	int 	i;

	numstatements = 1;
	strofs = 1;
	numfunctions = 1;
	numglobaldefs = 1;
	numfielddefs = 1;

	def_ret.ofs = OFS_RETURN;
	for (i = 0; i < MAX_PARMS; i++)
		def_parms[i].ofs = OFS_PARM0 + 3 * i;
}


void
WriteData (int crc)
{
	def_t		*def;
	ddef_t		*dd;
	dprograms_t progs;
	FILE		*h;
	int 		i;

	for (def = pr.def_head.next; def; def = def->next) {
		if (def->type->type == ev_func) {
//			df = &functions[numfunctions];
//			numfunctions++;
		} else if (def->type->type == ev_field) {
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = ReuseString (def->name);
			dd->ofs = G_INT (def->ofs);
		}

		dd = &globals[numglobaldefs];
		numglobaldefs++;
		dd->type = def->type->type;

		if (!def->initialized
			&& def->type->type != ev_func
			&& def->type->type != ev_field && def->scope == NULL)
			dd->type |= DEF_SAVEGLOBAL;
		dd->s_name = ReuseString (def->name);
		dd->ofs = def->ofs;
	}

//	PrintStrings ();
//	PrintFunctions ();
//	PrintFields ();
//	PrintGlobals ();
	strofs = (strofs + 3) & ~3;

	printf ("%6i strofs\n", strofs);
	printf ("%6i statements\n", numstatements);
	printf ("%6i functions\n", numfunctions);
	printf ("%6i globaldefs\n", numglobaldefs);
	printf ("%6i fielddefs\n", numfielddefs);
	printf ("%6i pr_globals\n", numpr_globals);

	h = SafeOpenWrite (destfile);
	SafeWrite (h, &progs, sizeof (progs));

	progs.ofs_strings = ftell (h);
	progs.numstrings = strofs;
	SafeWrite (h, strings, strofs);

	progs.ofs_statements = ftell (h);
	progs.numstatements = numstatements;
	for (i = 0; i < numstatements; i++) {
		statements[i].op = LittleShort (statements[i].op);
		statements[i].a = LittleShort (statements[i].a);
		statements[i].b = LittleShort (statements[i].b);
		statements[i].c = LittleShort (statements[i].c);
	}
	SafeWrite (h, statements, numstatements * sizeof (dstatement_t));

	progs.ofs_functions = ftell (h);
	progs.numfunctions = numfunctions;
	for (i = 0; i < numfunctions; i++) {
		functions[i].first_statement = LittleLong (functions[i].first_statement);
		functions[i].parm_start = LittleLong (functions[i].parm_start);
		functions[i].s_name = LittleLong (functions[i].s_name);
		functions[i].s_file = LittleLong (functions[i].s_file);
		functions[i].numparms = LittleLong (functions[i].numparms);
		functions[i].locals = LittleLong (functions[i].locals);
	}
	SafeWrite (h, functions, numfunctions * sizeof (dfunction_t));

	progs.ofs_globaldefs = ftell (h);
	progs.numglobaldefs = numglobaldefs;
	for (i = 0; i < numglobaldefs; i++) {
		globals[i].type = LittleShort (globals[i].type);
		globals[i].ofs = LittleShort (globals[i].ofs);
		globals[i].s_name = LittleLong (globals[i].s_name);
	}
	SafeWrite (h, globals, numglobaldefs * sizeof (ddef_t));

	progs.ofs_fielddefs = ftell (h);
	progs.numfielddefs = numfielddefs;
	for (i = 0; i < numfielddefs; i++) {
		fields[i].type = LittleShort (fields[i].type);
		fields[i].ofs = LittleShort (fields[i].ofs);
		fields[i].s_name = LittleLong (fields[i].s_name);
	}
	SafeWrite (h, fields, numfielddefs * sizeof (ddef_t));

	progs.ofs_globals = ftell (h);
	progs.numglobals = numpr_globals;

	for (i = 0; i < numpr_globals; i++)
		((int *) pr_globals)[i] = LittleLong (((int *) pr_globals)[i]);

	SafeWrite (h, pr_globals, numpr_globals * 4);

	printf ("%6i TOTAL SIZE\n", (int) ftell (h));

	progs.entityfields = pr.size_fields;

	progs.version = PROG_VERSION;
	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < sizeof (progs) / 4; i++)
		((int *) &progs)[i] = LittleLong (((int *) &progs)[i]);

	fseek (h, 0, SEEK_SET);
	SafeWrite (h, &progs, sizeof (progs));
	fclose (h);

}


/*
	PR_String

	Returns a string suitable for printing (no newlines, max 60 chars length)
*/
char *
PR_String (char *string)
{
	static char buf[80];
	char		*s;

	s = buf;
	*s++ = '"';
	while (string && *string) {

		if (s == buf + sizeof (buf) - 2)
			break;

		if (*string == '\n') {
			*s++ = '\\';
			*s++ = 'n';
		} else if (*string == '"') {
			*s++ = '\\';
			*s++ = '"';
		} else {
			*s++ = *string;
		}
		string++;

		if (s - buf > 60) {
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;

	return buf;
}



def_t *
PR_DefForFieldOfs (gofs_t ofs)
{
	def_t	*d;

	for (d = pr.def_head.next; d; d = d->next) {
		if (d->type->type != ev_field)
			continue;
		if (*((int *) &pr_globals[d->ofs]) == ofs)
			return d;
	}

	Error ("PR_DefForFieldOfs: couldn't find %i", ofs);
	return NULL;
}

/*
	PR_ValueString

	Return a string describing *data in a type-specific manner
*/
char *
PR_ValueString (etype_t type, void *val)
{
	static char line[256];
	def_t		*def;
	dfunction_t *f;

	switch (type) {
		case ev_string:
			sprintf (line, "%s", PR_String (strings + *(int *) val));
			break;
		case ev_entity:
			sprintf (line, "entity %i", *(int *) val);
			break;
		case ev_func:
			if (!(f = functions + *(int *) val))
				sprintf (line, "undefined function");
			else
				sprintf (line, "%s()", strings + f->s_name);
			break;
		case ev_field:
			def = PR_DefForFieldOfs (*(int *) val);
			sprintf (line, ".%s", def->name);
			break;
		case ev_void:
			sprintf (line, "void");
			break;
		case ev_float:
			sprintf (line, "%5.1f", *(float *) val);
			break;
		case ev_vector:
			sprintf (line, "'%5.1f %5.1f %5.1f'", ((float *) val)[0],
					 ((float *) val)[1], ((float *) val)[2]);
			break;
		case ev_pointer:
			sprintf (line, "pointer");
			break;
		default:
			sprintf (line, "bad type %i", type);
			break;
	}

	return line;
}

/*
	PR_GlobalString

	Return a string with a description and the contents of a global, padded
	to 20 field width
*/
char *
PR_GlobalStringNoContents (gofs_t ofs)
{
	int 		i;
	def_t		*def;
	void		*val;
	static char line[128];

	val = (void *) &pr_globals[ofs];
	def = pr_global_defs[ofs];
	if (!def) {
//		Error ("PR_GlobalString: no def for %i", ofs);
		sprintf (line, "%i(???)", ofs);
	} else {
		sprintf (line, "%i(%s)", ofs, def->name);
	}

	for (i = strlen (line); i < 16; i++) {
		strcat (line, " ");
	}
	strcat (line, " ");

	return line;
}

char *
PR_GlobalString (gofs_t ofs)
{
	char		*s;
	int 		i;
	def_t		*def;
	void		*val;
	static char line[128];

	val = (void *) &pr_globals[ofs];
	
	if (!(def = pr_global_defs[ofs]))
		return PR_GlobalStringNoContents (ofs);

	if (def->initialized && def->type->type != ev_func) {
		s = PR_ValueString (def->type->type, &pr_globals[ofs]);
		sprintf (line, "%i(%s)", ofs, s);
	} else {
		sprintf (line, "%i(%s)", ofs, def->name);
	}

	for (i = strlen (line); i < 16; i++) {
		strcat (line, " ");
	}
	strcat (line, " ");

	return line;
}

/*
============
PR_PrintOfs
============
*/
void
PR_PrintOfs (gofs_t ofs)
{
	printf ("%s\n", PR_GlobalString (ofs));
}

/*
=================
PR_PrintStatement
=================
*/
void
PR_PrintStatement (dstatement_t *s)
{
	int 	i;
	opcode_t *op;

	op = PR_Opcode (s->op);
	printf ("%4i : %4i : %s ", (int) (s - statements),
			statement_linenums[s - statements], op->opname);

	for (i = strlen (op->opname); i < 10; i++) {
		printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT) {
		printf ("%sbranch %i", PR_GlobalString (s->a), s->b);
	} else if (s->op == OP_GOTO) {
		printf ("branch %i", s->a);
	} else if ((unsigned) (s->op - OP_STORE_F) < 6) {
		printf ("%s", PR_GlobalString (s->a));
		printf ("%s", PR_GlobalStringNoContents (s->b));
	} else {
		if (s->a)
			printf ("%s", PR_GlobalString (s->a));
		if (s->b)
			printf ("%s", PR_GlobalString (s->b));
		if (s->c)
			printf ("%s", PR_GlobalStringNoContents (s->c));
	}
	printf ("\n");
}


/*
	PR_PrintDefs
*/
void
PR_PrintDefs (void)
{
	def_t	*d;

	for (d = pr.def_head.next; d; d = d->next)
		PR_PrintOfs (d->ofs);
}


/*
	PR_BeginCompilation

	called before compiling a batch of files, clears the pr struct
*/
void
PR_BeginCompilation (void *memory, int memsize)
{
	int 	i;

	pr.memory = memory;
	pr.max_memory = memsize;

	numpr_globals = RESERVED_OFS;
	pr.def_tail = &pr.def_head;

	for (i = 0; i < RESERVED_OFS; i++)
		pr_global_defs[i] = &def_void;

	// link the function type in so state forward declarations match proper type
	pr.types = &type_function;
	type_function.next = NULL;
	pr_error_count = 0;
}

/*
	PR_FinishCompilation

	called after all files are compiled to check for errors.
	Returns false if errors were detected.
*/
qboolean
PR_FinishCompilation (void)
{
	def_t		*d;
	qboolean	errors = false;

	// check to make sure all functions prototyped have code
	for (d = pr.def_head.next; d; d = d->next) {
		if (d->type->type == ev_func && !d->scope) {	// function args ok
//			f = G_FUNCTION(d->ofs);
//			if (!f || (!f->code && !f->builtin))
			if (!d->initialized) {
				printf ("function %s was not defined\n", d->name);
				errors = true;
			}
		}
	}

	return !errors;
}

//=============================================================================

/*
	PR_WriteProgdefs

	Writes the global and entity structures out.
	Returns a crc of the header, to be stored in the progs file for comparison
	at load time.
*/
int
PR_WriteProgdefs (char *filename)
{
	def_t			*d;
	FILE			*f;
	unsigned short	crc;
	int 			c;

	printf ("writing %s\n", filename);
	f = fopen (filename, "w");

	// print global vars until the first field is defined
	fprintf (f,	"\n/* file generated by qcc, do not modify */\n\ntypedef struct\n{\tint\tpad[%i];\n", RESERVED_OFS);

	for (d = pr.def_head.next; d; d = d->next) {
		if (!strcmp (d->name, "end_sys_globals"))
			break;

		switch (d->type->type) {
			case ev_float:
				fprintf (f, "\tfloat\t%s;\n", d->name);
				break;
			case ev_vector:
				fprintf (f, "\tvec3_t\t%s;\n", d->name);
				d = d->next->next->next;	// skip the elements
				break;
			case ev_string:
				fprintf (f, "\tstring_t\t%s;\n", d->name);
				break;
			case ev_func:
				fprintf (f, "\tfunc_t\t%s;\n", d->name);
				break;
			case ev_entity:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
			default:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
		}
	}
	fprintf (f, "} globalvars_t;\n\n");

	// print all fields
	fprintf (f, "typedef struct\n{\n");
	for (d = pr.def_head.next; d; d = d->next) {
		if (!strcmp (d->name, "end_sys_fields"))
			break;

		if (d->type->type != ev_field)
			continue;

		switch (d->type->aux_type->type) {
			case ev_float:
				fprintf (f, "\tfloat\t%s;\n", d->name);
				break;
			case ev_vector:
				fprintf (f, "\tvec3_t\t%s;\n", d->name);
				d = d->next->next->next;	// skip the elements
				break;
			case ev_string:
				fprintf (f, "\tstring_t\t%s;\n", d->name);
				break;
			case ev_func:
				fprintf (f, "\tfunc_t\t%s;\n", d->name);
				break;
			case ev_entity:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
			default:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
		}
	}
	fprintf (f, "} entvars_t;\n\n");

	fclose (f);

	// do a crc of the file
	CRC_Init (&crc);
	f = fopen (filename, "r+");
	while ((c = fgetc (f)) != EOF)
		CRC_ProcessByte (&crc, (byte) c);

	fprintf (f, "#define PROGHEADER_CRC %i\n", crc);
	fclose (f);

	return crc;
}


void
PrintFunction (char *name)
{
	int 			i;
	dstatement_t	*ds;
	dfunction_t 	*df;

	for (i = 0; i < numfunctions; i++)
		if (!strcmp (name, strings + functions[i].s_name))
			break;

	if (i == numfunctions)
		Error ("No function names \"%s\"", name);

	df = functions + i;

	printf ("Statements for %s:\n", name);
	ds = statements + df->first_statement;

	while (1) {
		PR_PrintStatement (ds);
		if (!ds->op)
			break;
		ds++;
	}
}


//============================================================================

/*
	main

	The nerve center of our little operation
*/
int
main (int argc, char **argv)
{
	char	*src;
	char	*src2;
	char	filename[1024];
	int 	p, crc;
	double	start, stop;

	start = Sys_DoubleTime ();

	myargc = argc;
	myargv = argv;

	if (CheckParm ("-h") || CheckParm ("--help")) {
		printf ("%s - A compiler for the QuakeC language\n", argv[0]);
		printf ("Usage: %s [options]\n", argv[0]);
		printf ("\
Options: \n\
	-s, --source <dir>	look for progs.src in directory <dir>\n\
	-h, --help		display this help and exit\n\
	-V, --version		output version information and exit\n\
");
		return 1;
	}

	if (CheckParm ("-V") || CheckParm ("--version")) {
		printf ("%s version %s\n", PACKAGE, VERSION);
		return 1;
	}

	if ((p = CheckParm ("--source")) && p < argc - 1) {
		strcpy (sourcedir, argv[p + 1]);
	} else {
		if ((p = CheckParm ("-s")) && p < argc - 1) {
			strcpy (sourcedir, argv[p + 1]);
		} else {
			strcpy (sourcedir, ".");
		}
	}

	if (strcmp (sourcedir, ".")) {
		printf ("Source directory: %s\n", sourcedir);
	}

	PR_Opcode_Init ();

	InitData ();

	sprintf (filename, "%s/progs.src", sourcedir);
	LoadFile (filename, (void *) &src);

	if (!(src = COM_Parse (src)))
		Error ("No destination filename.  qfcc --help for info.\n");

	strcpy (destfile, com_token);
	printf ("outputfile: %s\n", destfile);

	pr_dumpasm = false;

	PR_BeginCompilation (malloc (0x100000), 0x100000);

	// compile all the files
	while ((src = COM_Parse (src))) {
		sprintf (filename, "%s/%s", sourcedir, com_token);
		printf ("compiling %s\n", filename);
		LoadFile (filename, (void *) &src2);

		if (!PR_CompileFile (src2, filename))
			return 1;
	}

	if (!PR_FinishCompilation ())
		Error ("compilation errors");

	// write progdefs.h
	crc = PR_WriteProgdefs ("progdefs.h");

	// write data file
	WriteData (crc);

	// write files.dat
	WriteFiles ();

	stop = Sys_DoubleTime ();
	printf ("Compilation time: %.3f seconds.\n", (stop - start));
	return 0;
}
