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

#include <stdio.h>
#include <stdlib.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include "QF/progs.h"
#include <QF/quakeio.h>
#include <QF/sys.h>
#include <QF/va.h>
#include <QF/zone.h>

#include "def.h"

static edict_t *edicts;
static int      num_edicts;
static int      reserved_edicts = 1;
static progs_t  pr;
static void    *membase;
static int      memsize = 1024*1024;

static QFile *
open_file (const char *path, int *len)
{
	QFile      *file = Qopen (path, "rbz");
	if (!file) {
		perror (path);
		return 0;
	}
	*len = Qfilesize (file);
	return file;
}

static void *
load_file (progs_t *pr, const char *name)
{
	QFile      *file;
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

	PR_Init_Cvars ();
	PR_Init ();

	pr.edicts = &edicts;
	pr.num_edicts = &num_edicts;
	pr.reserved_edicts = &reserved_edicts;
	pr.load_file = load_file;
	pr.allocate_progs_mem = allocate_progs_mem;
	pr.free_progs_mem = free_progs_mem;
}

int
load_progs (const char *name)
{
	QFile      *file;
	int         size;

	file = open_file (name, &size);
	if (!file) {
		perror (name);
		return 0;
	}
	pr.progs_name = name;
	PR_LoadProgsFile (&pr, file, size, 1, 0);
	Qclose (file);
	PR_LoadStrings (&pr);
	return 1;
}

int
main (int argc, char **argv)
{
	def_t *globals, *fields, *def;

	if (argc < 2) {
		fprintf (stderr, "usage: qfdefs <progs> ...\n");
		return 1;
	}
	init_qf ();
	while (--argc) {
		int fix = 0;

		if (!load_progs (*++argv)) {
			fprintf (stderr, "failed to load %s\n", *argv);
			return 1;
		}

		if (pr.progs->version < 0x100)
			printf ("%s: version %d\n", pr.progs_name, pr.progs->version);
		else
			printf ("%s: version %x.%03x.%03x\n", pr.progs_name,
					(pr.progs->version >> 24) & 0xff,
					(pr.progs->version >> 12) & 0xfff,
					pr.progs->version & 0xfff);
		if (pr.progs->crc == nq_crc) {
			printf ("%s: netquake crc\n", pr.progs_name);
			Init_Defs (nq_global_defs, nq_field_defs);
			globals = nq_global_defs;
			fields = nq_field_defs;
		} else if (pr.progs->crc == qw_crc) {
			printf ("%s: quakeworld crc\n", pr.progs_name);
			Init_Defs (qw_global_defs, qw_field_defs);
			globals = qw_global_defs;
			fields = qw_field_defs;
		} else {
			printf ("%s: unknown crc %d\n", pr.progs_name, pr.progs->crc);
			continue;
		}
		for (def = globals; def->name; def++)
			if (!PR_FindGlobal (&pr, def->name))
				break;
		if (!def->name)
			printf ("%s: all system globals accounted for\n", pr.progs_name);
		else {
			printf ("%s: some system globals missing\n", pr.progs_name);
			fix_missing_globals (&pr, globals);
			fix++;
		}
		for (def = fields; def->name; def++)
			if (!ED_FindField (&pr, def->name))
				break;
		if (!def->name)
			printf ("%s: all system fields accounted for\n", pr.progs_name);
		else {
			printf ("%s: some system fields missing\n", pr.progs_name);
		}
		if (fix) {
			//XXX FIXME endian fixups
			FILE *f = fopen (va ("%s.new", pr.progs_name), "wb");
			fwrite (pr.progs, pr.progs_size, 1, f);
			fclose (f);
		}
	}
	return 0;
}
