#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <QF/progs.h>

#include "def.h"

extern char *type_name[];

void
update_offsets (dprograms_t *pr, int mark, int count)
{
	if (pr->ofs_statements > mark)
		pr->ofs_statements += count;
	if (pr->ofs_globaldefs > mark)
		pr->ofs_globaldefs += count;
	if (pr->ofs_fielddefs > mark)
		pr->ofs_fielddefs += count;
	if (pr->ofs_functions > mark)
		pr->ofs_functions += count;
	if (pr->ofs_strings > mark)
		pr->ofs_strings += count;
	if (pr->ofs_globals > mark)
		pr->ofs_globals += count;
}

void
fix_missing_globals (progs_t *pr, def_t *globals)
{
	int defs_count = 0;
	int strings_size = 0;
	int defs_size;
	int i;
	def_t *def;
	ddef_t **new_defs, *d, **n;
	char *new_strings;
	dprograms_t *progs;
	int offs;

	for (def = globals; def->name; def ++) {
		if (!PR_FindGlobal (pr, def->name)) {
			defs_count++;
		}
	}
	n = new_defs = calloc (defs_count, sizeof (def_t **));
	for (def = globals; def->name; def ++) {
		d = ED_GlobalAtOfs (pr, def->offset);
		if (d) {
			if (d->type != (def->type)) {
				fprintf (stderr,
						 "global def %s at %d has mismatched type (%s, %s)\n",
						 def->name, def->offset,
						 type_name[def->type & ~DEF_SAVEGLOBAL],
						 type_name[d->type & ~DEF_SAVEGLOBAL]);
				exit (1);
			}
			if (strcmp (def->name, PR_GetString (pr, d->s_name)) == 0)
				continue;	// this one is ok
			d->s_name = strings_size;
			*n++ = d;
			strings_size += strlen (def->name) + 1;
		} else {
			*n = calloc (sizeof (ddef_t), 1);
			(*n)->type = (def->type | DEF_SAVEGLOBAL);
			(*n)->ofs = def->offset;
			(*n)->s_name = strings_size;
			n++;
			strings_size += strlen (def->name) + 1;
		}
	}
	new_strings = calloc (strings_size, 1);
	for (i = 0; i < defs_count; i++) {
		def = Find_Global_Def_offs (new_defs[i]->ofs);
		strcpy (new_strings + new_defs[i]->s_name, def->name);
		new_defs[i]->s_name += pr->pr_stringsize;
	}

	defs_size = defs_count * sizeof (ddef_t);
	progs = malloc (pr->progs_size + strings_size + defs_size);
	memcpy (progs, pr->progs, pr->progs_size);

	offs = progs->ofs_globaldefs + progs->numglobaldefs * sizeof (ddef_t);

	memmove ((char*)progs + offs + defs_size, (char*)progs + offs,
			 pr->progs_size - offs);
	memcpy ((char*)progs + offs, new_defs, defs_size);

	progs->numglobaldefs += defs_count;
	pr->progs_size += defs_size;
	update_offsets (progs, progs->ofs_globaldefs, defs_size);

	offs = progs->ofs_strings + progs->numstrings;
	memmove ((char*)progs + offs + strings_size, (char*)progs + offs,
			 pr->progs_size - offs);
	memcpy ((char*)progs + offs, new_strings, strings_size);

	progs->numstrings += strings_size;
	pr->progs_size += strings_size;
	update_offsets (progs, progs->ofs_strings, strings_size);

	pr->progs = progs;
}
