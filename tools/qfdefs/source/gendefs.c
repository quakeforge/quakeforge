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
#include <ctype.h>
#include <string.h>

int offset;

static void
output_def (FILE *out, const char *line)
{
	const char *type, *type_e;
	const char *name, *name_e;

	for (type = line; *type && isspace (*type); type++)
		;
	for (type_e = type; *type_e && !isspace (*type_e); type_e++)
		;
	for (name = type_e; *name && isspace (*name); name++)
		;
	for (name_e = name; *name_e && !isspace (*name_e) && *name_e != ';'; name_e++)
		;
	if (strncmp ("int", type, type_e - type) == 0) {
		fprintf (out, "\t{ev_entity | DEF_SAVEGLOBAL,\t%d,\t\"%.*s\"},\n",
				 offset, (int) (name_e - name), name);
		offset += 1;
	} else if (strncmp ("float", type, type_e - type) == 0) {
		fprintf (out, "\t{ev_float | DEF_SAVEGLOBAL,\t%d,\t\"%.*s\"},\n",
				 offset, (int) (name_e - name), name);
		offset += 1;
	} else if (strncmp ("string_t", type, type_e - type) == 0) {
		fprintf (out, "\t{ev_string | DEF_SAVEGLOBAL,\t%d,\t\"%.*s\"},\n",
				 offset, (int) (name_e - name), name);
		offset += 1;
	} else if (strncmp ("vec3_t", type, type_e - type) == 0) {
		fprintf (out, "\t{ev_vector | DEF_SAVEGLOBAL,\t%d,\t\"%.*s\"},\n",
				 offset, (int) (name_e - name), name);
		offset += 3;
	} else if (strncmp ("func_t", type, type_e - type) == 0) {
		fprintf (out, "\t{ev_func,\t%d,\t\"%.*s\"},\n",
				 offset, (int) (name_e - name), name);
		offset += 1;
	} else {
		fprintf (stderr, "unknown type %.*s\n", (int) (type_e - type), type);
		exit (1);
	}
}

int
main (int argc, char **argv)
{
	FILE *in, *out;
	char buf[256];
	int state = 0;

	if (argc != 4) {
		fprintf (stderr, "usage: gendefs prefix infile outfile\n");
		return 1;
	}
	if (!(in = fopen (argv[2], "rt"))) {
		perror (argv[2]);
		return 1;
	}
	if (!(out = fopen (argv[3], "wt"))) {
		perror (argv[3]);
		return 1;
	}
	fputs ("#include \"def.h\"\n\n", out);
	fprintf (out, "static const char rcsid[] =\n\t\"%s\";\n", rcsid);
	fprintf (out, "static const void *__rcsid_hack (void)\n");
	fprintf (out, "{ __rcsid_hack (); return rcsid; }\n");
	while (fgets (buf, sizeof (buf), in)) {
		switch (state) {
			case 0:
				if (buf[0] == '{')
					state++;
				break;
			case 1:
				fprintf (out, "def_t %s_global_defs[] = {\n", argv[1]);
				offset = 28;
				state++;
			case 2:
				if (buf[0] == '}') {
					fputs ("\t{ev_void,\t0,\t0},\n", out);
					fputs ("};\n", out);
					state++;
					break;
				}
				output_def (out, buf);
				break;
			case 3:
				if (buf[0] == '{')
					state++;
				break;
			case 4:
				fputs ("\n", out);
				fprintf (out, "def_t %s_field_defs[] = {\n", argv[1]);
				offset = 0;
				state++;
			case 5:
				if (buf[0] == '}') {
					state++;
					break;
				}
				output_def (out, buf);
				break;
			case 6:
				fputs ("\t{ev_void,\t0,\t0},\n", out);
				fputs ("};\n", out);
				state++;
				break;
			case 7:
				if (strncmp ("#define", buf, 7) == 0) {
					int crc;
					sscanf (buf, "#define PROGHEADER_CRC %d", &crc);
					fprintf (out, "\nint %s_crc = %d;\n", argv[1], crc);
				}
				break;
		}
	}
	return 0;
}
