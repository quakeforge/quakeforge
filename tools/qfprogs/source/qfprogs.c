/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <getopt.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <getopt.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/pr_comp.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vfile.h"
#include "QF/zone.h"

#include "disassemble.h"
#include "globals.h"
#include "strings.h"

static const struct option long_options[] = {
	{"disassemble", no_argument, 0, 'd'},
	{"globals", no_argument, 0, 'g'},
	{"strings", no_argument, 0, 's'},
	{"fields", no_argument, 0, 'f'},
	{"functions", no_argument, 0, 'F'},
	{NULL, 0, NULL, 0},
};

static edict_t *edicts;
static int      num_edicts;
static int      reserved_edicts = 1;
static progs_t  pr;
static void    *membase;
static int      memsize = 1024*1024;

static hashtab_t *func_tab;

static VFile *
open_file (const char *path, int *len)
{
	int         fd = open (path, O_RDONLY);
	unsigned char id[2];
	unsigned char len_bytes[4];

	if (fd == -1) {
		perror (path);
		return 0;
	}
	read (fd, id, 2);
	if (id[0] == 0x1f && id[1] == 0x8b) {
		lseek (fd, -4, SEEK_END);
		read (fd, len_bytes, 4);
		*len = ((len_bytes[3] << 24)
				| (len_bytes[2] << 16)
				| (len_bytes[1] << 8)
				| (len_bytes[0]));
	} else {
		*len = lseek (fd, 0, SEEK_END);
	}
	lseek (fd, 0, SEEK_SET);

	return Qdopen (fd, "rbz");
}

static void *
load_file (progs_t *pr, const char *name)
{
	VFile      *file;
	int         size;
	void       *sym;

	file = open_file (name, &size);
	if (!file) {
		file = open_file (va ("%s.gz", name), &size);
		if (!file) {
			return 0;
		}
	}
	sym = malloc (size);
	Qread (file, sym, size);
	return sym;
}

static void *
allocate_progs_mem (progs_t *pr, int size)
{
	return malloc (size);
}

static void
free_progs_mem (progs_t *pr, void *mem)
{
	free (mem);
}

static unsigned long
func_hash (void *func, void *unused)
{
	return ((dfunction_t *) func)->first_statement;
}

static int
func_compare (void *f1, void *f2, void *unused)
{
	return ((dfunction_t *) f1)->first_statement
			== ((dfunction_t *) f2)->first_statement;
}

dfunction_t *
func_find (int st_ofs)
{
	dfunction_t f;

	f.first_statement = st_ofs;
	return Hash_FindElement (func_tab, &f);
}

static void
init_qf (void)
{
	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cvar_Init ();
	Sys_Init_Cvars ();
	Cmd_Init ();

	membase = malloc (memsize);
	Memory_Init (membase, memsize);

	Cvar_Get ("pr_debug", "1", 0, 0, "");
	PR_Init_Cvars ();
	PR_Init ();

	pr.edicts = &edicts;
	pr.num_edicts = &num_edicts;
	pr.reserved_edicts = &reserved_edicts;
	pr.load_file = load_file;
	pr.allocate_progs_mem = allocate_progs_mem;
	pr.free_progs_mem = free_progs_mem;

	func_tab = Hash_NewTable (1021, 0, 0, 0);
	Hash_SetHashCompare (func_tab, func_hash, func_compare);
}

int
load_progs (const char *name)
{
	VFile      *file;
	int         i, size;

	file = open_file (name, &size);
	if (!file) {
		perror (name);
		return 0;
	}
	Hash_FlushTable (func_tab);
	pr.progs_name = name;
	PR_LoadProgsFile (&pr, file, size, 0, 0);
	Qclose (file);
	PR_LoadStrings (&pr);

	*pr.edicts = PR_InitEdicts (&pr, 1);

	for (i = 0; i < pr.progs->numfunctions; i++) {
		if (pr.pr_functions[i].first_statement > 0)// don't bother with builtins
			Hash_AddElement (func_tab, &pr.pr_functions[i]);
	}
	PR_LoadDebug (&pr);
	return 1;
}

int
main (int argc, char **argv)
{
	int         c;
	void      (*func)(progs_t *pr) = dump_globals;

	init_qf ();
	while ((c = getopt_long (argc, argv, "dgsfF", long_options, 0)) != EOF) {
		switch (c) {
			case 'd':
				func = disassemble_progs;
				break;
			case 'g':
				func = dump_globals;
				break;
			case 's':
				func = dump_strings;
				break;
			case 'f':
				func = dump_fields;
				break;
			case 'F':
				func = dump_functions;
				break;
			default:
				return 1;
		}
	}
	while (optind < argc) {
		load_progs (argv[optind++]);
		func (&pr);
	}
	return 0;
}
