#include <QF/hash.h>

#include "def.h"

static hashtab_t *global_defs_by_name;
static hashtab_t *global_defs_by_offs;
static hashtab_t *field_defs_by_name;
static hashtab_t *field_defs_by_offs;

static const char *
get_key_name (void *d, void *unused)
{
	return ((def_t*)d)->name;
}

static const char *
get_key_offs (void *_d, void *unused)
{
	def_t *d = (def_t*)_d;
	static char rep[4];
	rep[0] = (d->offset & 0x7f) + 1;
	rep[1] = ((d->offset >> 7) & 0x7f) + 1;
	rep[2] = ((d->offset >> 14) & 0x7f) + 1;
	return rep;
}

void
Init_Defs (def_t *gtab, def_t *ftab)
{
	if (global_defs_by_name)
		Hash_FlushTable (global_defs_by_name);
	else
		global_defs_by_name = Hash_NewTable (1021, get_key_name, 0, 0);
	if (global_defs_by_offs)
		Hash_FlushTable (global_defs_by_offs);
	else
		global_defs_by_offs = Hash_NewTable (1021, get_key_offs, 0, 0);
	while (gtab->name) {
		Hash_Add (global_defs_by_name, gtab);
		Hash_Add (global_defs_by_offs, gtab);
		gtab++;
	}

	if (field_defs_by_name)
		Hash_FlushTable (field_defs_by_name);
	else
		field_defs_by_name = Hash_NewTable (1021, get_key_name, 0, 0);
	if (field_defs_by_offs)
		Hash_FlushTable (field_defs_by_offs);
	else
		field_defs_by_offs = Hash_NewTable (1021, get_key_offs, 0, 0);
	while (ftab->name) {
		Hash_Add (field_defs_by_name, ftab);
		Hash_Add (field_defs_by_offs, ftab);
		ftab++;
	}
}

def_t *
Find_Global_Def_name (const char *name)
{
	return Hash_Find (global_defs_by_name, name);
}

def_t *
Find_Global_Def_offs (int offs)
{
	def_t d;
	char rep[4];

	d.offset = offs;
	strcpy (rep, get_key_offs (&d, 0));
	return Hash_Find (global_defs_by_offs, rep);
}

def_t *
Find_Field_Def_name (const char *name)
{
	return Hash_Find (field_defs_by_name, name);
}

def_t *
Find_Field_Def_offs (int offs)
{
	def_t d;
	char rep[4];

	d.offset = offs;
	strcpy (rep, get_key_offs (&d, 0));
	return Hash_Find (field_defs_by_offs, rep);
}
