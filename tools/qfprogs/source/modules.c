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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/pr_obj.h"
#include "QF/progs.h"
#include "QF/va.h"

#include "qfprogs.h"
#include "modules.h"

void
dump_methods (progs_t *pr, pr_method_list_t *methods, int class)
{
	int         i;
	char        mark = class ? '+' : '-';

	while (methods) {
		pr_method_t *method = methods->method_list;
		for (i = 0; i < methods->method_count; i++) {
			printf ("        %c%s %d @ %d\n", mark,
					PR_GetString (pr, method->method_name.sel_id),
					method->method_imp, POINTER_TO_PROG (pr, method));
			method++;
		}
		methods = &G_STRUCT (pr, pr_method_list_t, methods->method_next);
	}
}

void
dump_class (progs_t *pr, pr_class_t *class)
{
	pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);

	if (class->super_class) {
		printf ("    %s @ %d : %s\n", PR_GetString (pr, class->name),
				POINTER_TO_PROG (pr, class), PR_GetString (pr,
														   class->super_class));
	} else {
		printf ("    %s @ %d\n", PR_GetString (pr, class->name),
				POINTER_TO_PROG (pr, class));
	}
	printf ("        %d %d %d %d\n", class->class_pointer, class->version,
			class->info, class->instance_size);
	dump_methods (pr, &G_STRUCT (pr, pr_method_list_t, class->methods), 0);
	dump_methods (pr, &G_STRUCT (pr, pr_method_list_t, meta->methods), 1);
}

void
dump_category (progs_t *pr, pr_category_t *category)
{
}

void
dump_module (progs_t *pr, pr_module_t *module)
{
	pr_symtab_t *symtab = &G_STRUCT (pr, pr_symtab_t, module->symtab);
	pointer_t  *ptr = symtab->defs;
	int         i;

	printf ("%d %d %s\n", module->version, module->size,
			PR_GetString (pr, module->name));
	if (!symtab) {
		printf ("    No symtab!\n");
		return;
	}
	printf ("    %d %d %d\n", symtab->sel_ref_cnt, symtab->cls_def_cnt,
			symtab->cat_def_cnt);
	for (i = 0; i < symtab->cls_def_cnt; i++)
		dump_class (pr, &G_STRUCT (pr, pr_class_t, *ptr++));
	for (i = 0; i < symtab->cls_def_cnt; i++)
		dump_category (pr, &G_STRUCT (pr, pr_category_t, *ptr++));
}

void
dump_modules (progs_t *pr)
{
	int i;
	const char *name;

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		ddef_t *def = &pr->pr_globaldefs[i];

		name = PR_GetString (pr, def->s_name);
		if (strcmp (name, "_OBJ_MODULE") == 0)
			dump_module (pr, &G_STRUCT (pr, pr_module_t, def->ofs));
	}
}
