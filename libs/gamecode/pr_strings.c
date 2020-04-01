/*
	pr_strings.c

	progs string managment

	Copyright (C) 1996-1997  Id Software, Inc.

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/progs.h"

// format adjustments
#define FMT_ALTFORM		(1<<0)
#define FMT_LJUSTIFY	(1<<1)
#define FMT_ZEROPAD		(1<<2)
#define FMT_ADDSIGN		(1<<3)
#define FMT_ADDBLANK	(1<<4)
#define FMT_HEX			(1<<5)
#define FMT_LONG		(1<<6)

typedef struct fmt_item_s {
	byte        type;
	unsigned    flags;
	int         minFieldWidth;
	int         precision;
	union {
		const char *string_var;
		int         integer_var;
		unsigned    uinteger_var;
		float       float_var;
		double      double_var;
	}           data;
	struct fmt_item_s *next;
} fmt_item_t;

typedef struct strref_slot_s {
	struct strref_slot_s *next;
	struct strref_slot_s **prev;
	strref_t   *strref;
} strref_slot_t;

typedef struct prstr_resources_s {
	progs_t    *pr;
	dstring_mem_t ds_mem;
	strref_t   *free_string_refs;
	strref_t   *static_strings;
	strref_t  **string_map;
	strref_slot_t return_strings[PR_RS_SLOTS];
	strref_slot_t *rs_slot;
	unsigned    dyn_str_size;
	struct hashtab_s *strref_hash;
	int         num_strings;
	fmt_item_t *free_fmt_items;
	dstring_t  *print_str;
} prstr_resources_t;

typedef enum {
	str_free,
	str_static,
	str_dynamic,
	str_mutable,
	str_temp,
	str_return,
} str_e;

struct strref_s {
	strref_t   *next;
	strref_slot_t *rs_slot;
	str_e       type;
	union {
		char       *string;
		dstring_t  *dstring;
	} s;
};

static void *
pr_strings_alloc (void *_pr, size_t size)
{
	progs_t    *pr = (progs_t *) _pr;
	return PR_Zone_Malloc (pr, size);
}

static void
pr_strings_free (void *_pr, void *ptr)
{
	progs_t    *pr = (progs_t *) _pr;
	PR_Zone_Free (pr, ptr);
}

static void *
pr_strings_realloc (void *_pr, void *ptr, size_t size)
{
	progs_t    *pr = (progs_t *) _pr;
	return PR_Zone_Realloc (pr, ptr, size);
}

static strref_t *
new_string_ref (prstr_resources_t *res)
{
	strref_t *sr;
	if (!res->free_string_refs) {
		int		    i;
		size_t      size;

		res->dyn_str_size++;
		size = res->dyn_str_size * sizeof (strref_t *);
		res->string_map = realloc (res->string_map, size);
		if (!res->string_map)
			PR_Error (res->pr, "out of memory");
		if (!(res->free_string_refs = calloc (1024, sizeof (strref_t))))
			PR_Error (res->pr, "out of memory");
		res->string_map[res->dyn_str_size - 1] = res->free_string_refs;
		for (i = 0, sr = res->free_string_refs; i < 1023; i++, sr++)
			sr->next = sr + 1;
		sr->next = 0;
	}
	sr = res->free_string_refs;
	res->free_string_refs = sr->next;
	sr->next = 0;
	sr->rs_slot = 0;
	return sr;
}

static void
free_string_ref (prstr_resources_t *res, strref_t *sr)
{
	sr->type = str_free;
	sr->next = res->free_string_refs;
	res->free_string_refs = sr;
}

static __attribute__((pure)) string_t
string_index (prstr_resources_t *res, strref_t *sr)
{
	long        o = (long) (sr - res->static_strings);
	unsigned    i;

	if (o >= 0 && o < res->num_strings)
		return sr->s.string - res->pr->pr_strings;
	for (i = 0; i < res->dyn_str_size; i++) {
		int         d = sr - res->string_map[i];
		if (d >= 0 && d < 1024)
			return ~(i * 1024 + d);
	}
	return 0;
}

static const char *
strref_get_key (const void *_sr, void *notused)
{
	strref_t	*sr = (strref_t*)_sr;

	// only static strings will ever be in the hash table
	return sr->s.string;
}

static void
strref_free (void *_sr, void *_res)
{
	__auto_type res = (prstr_resources_t *) _res;
	__auto_type sr = (strref_t *) _sr;

	// Since this is called only by Hash_FlushTable, the memory pointed
	// to by sr->string or sr->dstring has already been lost in the progs
	// load/reload and thus there's no need to free it.

	// free the string and ref only if it's not a static string
	if (sr < res->static_strings
		|| sr >= res->static_strings + res->num_strings) {
		free_string_ref (res, sr);
	}
}

static void
pr_strings_clear (progs_t *pr, void *data)
{
	__auto_type res = (prstr_resources_t *) data;
	int         i;

	for (i = 0; i < PR_RS_SLOTS; i++) {
		if (res->return_strings[i].strref)
			free_string_ref (res, res->return_strings[i].strref);
		res->return_strings[i].strref = 0;
	}
	if (!res->rs_slot) {
		strref_slot_t * const rs = res->return_strings;
		for (i = 0; i < PR_RS_SLOTS; i++) {
			rs[i].next = &rs[(i + 1) % PR_RS_SLOTS];
			rs[i].prev = &rs[(i - 1 + PR_RS_SLOTS) % PR_RS_SLOTS].next;
		}
		res->rs_slot = rs;
	}

	pr->pr_string_resources = res;
	pr->pr_xtstr = 0;
}

static int
PR_LoadStrings (progs_t *pr)
{
	prstr_resources_t *res = PR_Resources_Find (pr, "Strings");

	char   *end = pr->pr_strings + pr->progs->numstrings;
	char   *str = pr->pr_strings;
	int		count = 0;

	pr->float_promoted = 0;

	while (str < end) {
		count++;
		if (*str == '@' && pr->progs->version == PROG_VERSION) {
			if (!strcmp (str, "@float_promoted@")) {
				pr->float_promoted = 1;
			}
		}
		str += strlen (str) + 1;
	}

	res->ds_mem.alloc = pr_strings_alloc;
	res->ds_mem.free = pr_strings_free;
	res->ds_mem.realloc = pr_strings_realloc;
	res->ds_mem.data = pr;

	if (res->strref_hash) {
		Hash_FlushTable (res->strref_hash);
	} else {
		res->strref_hash = Hash_NewTable (1021, strref_get_key, strref_free,
										  res, pr->hashlink_freelist);
		res->string_map = 0;
		res->free_string_refs = 0;
		res->dyn_str_size = 0;
	}

	if (res->static_strings)
		free (res->static_strings);
	res->static_strings = calloc (count, sizeof (strref_t));
	count = 0;
	str = pr->pr_strings;
	while (str < end) {
		if (!Hash_Find (res->strref_hash, str)) {
			res->static_strings[count].type = str_static;
			res->static_strings[count].s.string = str;
			Hash_Add (res->strref_hash, &res->static_strings[count]);
			count++;
		}
		str += strlen (str) + 1;
	}
	res->num_strings = count;
	return 1;
}

static void
requeue_strref (prstr_resources_t *res, strref_t *sr)
{
	strref_slot_t *rs_slot = sr->rs_slot;
	if (rs_slot->next != res->rs_slot) {
		// this is the oldest slot, so advance res->rs_slot to the
		// next oldest slot so this slot does not get reused just yet
		if (res->rs_slot == rs_slot) {
			res->rs_slot = rs_slot->next;
		}
		// unlink this slot from the chain
		rs_slot->next->prev = rs_slot->prev;
		*rs_slot->prev = rs_slot->next;
		// link this slot just before the oldest slot: all the slots
		// form a doubly linked circular list
		rs_slot->prev = res->rs_slot->prev;
		rs_slot->next = res->rs_slot;
		*res->rs_slot->prev = rs_slot;
		res->rs_slot->prev = &rs_slot->next;
	}
}

static inline strref_t *
get_strref (prstr_resources_t *res, string_t num)
{
	if (num < 0) {
		strref_t   *ref;
		unsigned    row = ~num / 1024;

		num = ~num % 1024;

		if (row >= res->dyn_str_size)
			return 0;
		ref = &res->string_map[row][num];
		if (ref->type == str_free)
			return 0;
		return ref;
	}
	return 0;
}

static inline __attribute__((pure)) const char *
get_string (progs_t *pr, string_t num)
{
	__auto_type res = pr->pr_string_resources;
	if (num < 0) {
		strref_t   *ref = get_strref (res, num);
		if (!ref)
			return 0;
		switch (ref->type) {
			case str_return:
				requeue_strref (res, ref);
			case str_static:
			case str_temp:
			case str_dynamic:
				return ref->s.string;
			case str_mutable:
				return ref->s.dstring->str;
			case str_free:
				break;
		}
		PR_Error (pr, "internal string error: %d", __LINE__);
	} else {
		if (num >= pr->pr_stringsize)
			return 0;
		return pr->pr_strings + num;
	}
}

VISIBLE qboolean
PR_StringValid (progs_t *pr, string_t num)
{
	if (num >= 0) {
		return num < pr->pr_stringsize;
	}
	return get_strref (pr->pr_string_resources, num) != 0;
}

VISIBLE qboolean
PR_StringMutable (progs_t *pr, string_t num)
{
	strref_t   *sr;
	if (num >= 0) {
		return 0;
	}
	sr = get_strref (pr->pr_string_resources, num);
	return  sr && sr->type == str_mutable;
}

VISIBLE const char *
PR_GetString (progs_t *pr, string_t num)
{
	const char *str;

	str = get_string (pr, num);
	if (str)
		return str;
	PR_RunError (pr, "Invalid string offset %d", num);
}

VISIBLE dstring_t *
PR_GetMutableString (progs_t *pr, string_t num)
{
	strref_t   *ref = get_strref (pr->pr_string_resources, num);
	if (ref) {
		if (ref->type == str_mutable)
			return ref->s.dstring;
		PR_RunError (pr, "not a dstring: %d", num);
	}
	PR_RunError (pr, "Invalid string offset: %d", num);
}

static inline void *
pr_strmalloc (progs_t *pr, size_t size)
{
	return PR_Zone_Malloc (pr, size);
}

static inline char *
pr_stralloc (progs_t *pr, size_t len)
{
	return pr_strmalloc (pr, len + 1);
}

static inline void
pr_strfree (progs_t *pr, char *s)
{
	PR_Zone_Free (pr, s);
}

static inline char *
pr_strdup (progs_t *pr, const char *s)
{
	char       *new = pr_stralloc (pr, strlen (s));
	strcpy (new, s);
	return new;
}

VISIBLE string_t
PR_SetString (progs_t *pr, const char *s)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr;

	if (!s)
		s = "";
	sr = Hash_Find (res->strref_hash, s);

	if (__builtin_expect (!sr, 1)) {
		sr = new_string_ref (res);
		sr->type = str_static;
		sr->s.string = pr_strdup(pr, s);
		Hash_Add (res->strref_hash, sr);
	}
	return string_index (res, sr);
}

VISIBLE string_t
PR_SetReturnString (progs_t *pr, const char *s)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr;

	if (!s)
		s = "";
	if ((sr = Hash_Find (res->strref_hash, s))) {
		if (sr->type == str_return && sr->rs_slot) {
			requeue_strref (res, sr);
		} else if ((sr->type == str_return && !sr->rs_slot)
				   || (sr->type != str_return && sr->rs_slot)) {
			PR_Error (pr, "internal string error: %d %d %p", __LINE__,
					 sr->type, sr->rs_slot);
		}
		return string_index (res, sr);
	}

	// grab the string ref from the oldest slot, or make a new one if the
	// slot is empty or the string has been held
	if ((sr = res->rs_slot->strref) && sr->type != str_dynamic) {
		if (sr->type != str_return || sr->rs_slot != res->rs_slot) {
			PR_Error (pr, "internal string error: %d", __LINE__);
		}
		pr_strfree (pr, sr->s.string);
	} else {
		sr = new_string_ref (res);
		res->rs_slot->strref = sr;
	}
	sr->type = str_return;
	sr->rs_slot = res->rs_slot;
	sr->s.string = pr_strdup(pr, s);

	// the oldest slot just became the newest, so advance to the next oldest
	res->rs_slot = res->rs_slot->next;

	return string_index (res, sr);
}

static inline string_t
pr_settempstring (progs_t *pr, prstr_resources_t *res, char *s)
{
	strref_t   *sr;

	sr = new_string_ref (res);
	sr->type = str_temp;
	sr->s.string = s;
	sr->next = pr->pr_xtstr;
	pr->pr_xtstr = sr;
	return string_index (res, sr);
}

VISIBLE string_t
PR_CatStrings (progs_t *pr, const char *a, const char *b)
{
	size_t      lena;
	char       *c;

	lena = strlen (a);

	c = pr_stralloc (pr, lena + strlen (b));

	strcpy (c, a);
	strcpy (c + lena, b);

	return pr_settempstring (pr, pr->pr_string_resources, c);
}

VISIBLE string_t
PR_SetTempString (progs_t *pr, const char *s)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr;

	if (!s)
		return PR_SetString (pr, "");

	if ((sr = Hash_Find (res->strref_hash, s))) {
		return string_index (res, sr);
	}

	return pr_settempstring (pr, res, pr_strdup (pr, s));
}

VISIBLE string_t
PR_AllocTempBlock (progs_t *pr, size_t size)
{
	prstr_resources_t *res = pr->pr_string_resources;
	return pr_settempstring (pr, res, pr_strmalloc (pr, size));
}

VISIBLE void
PR_PushTempString (progs_t *pr, string_t num)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *ref = get_strref (res, num);
	strref_t  **temp_ref;

	if (!ref || ref->type != str_temp) {
		PR_Error (pr, "attempt to push a non-temp string");
	}
	for (temp_ref = &pr->pr_xtstr; *temp_ref; temp_ref = &(*temp_ref)->next) {
		if (*temp_ref == ref) {
			*temp_ref = ref->next;
			ref->next = pr->pr_pushtstr;
			pr->pr_pushtstr = ref;
			return;
		}
	}
	PR_Error (pr, "attempt to push stale temp string");
}

VISIBLE string_t
PR_SetDynamicString (progs_t *pr, const char *s)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr;

	if (!s)
		return PR_SetString (pr, "");

	if ((sr = Hash_Find (res->strref_hash, s))) {
		return string_index (res, sr);
	}

	sr = new_string_ref (res);
	sr->type = str_dynamic;
	sr->s.string = pr_strdup (pr, s);
	return string_index (res, sr);
}

VISIBLE void
PR_MakeTempString (progs_t *pr, string_t str)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr = get_strref (res, str);

	if (!sr)
		PR_RunError (pr, "invalid string %d", str);
	if (sr->type != str_mutable)
		PR_RunError (pr, "not a dstring: %d", str);
	if (sr->s.dstring->str) {
		sr->s.string = dstring_freeze (sr->s.dstring);
	} else {
		dstring_delete (sr->s.dstring);
	}
	if (!sr->s.string)
		sr->s.string = pr_strdup (pr, "");
	sr->type = str_temp;
	sr->next = pr->pr_xtstr;
	pr->pr_xtstr = sr;
}

VISIBLE string_t
PR_NewMutableString (progs_t *pr)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr = new_string_ref (res);
	sr->type = str_mutable;
	sr->s.dstring = _dstring_newstr (&res->ds_mem);
	return string_index (res, sr);
}

VISIBLE void
PR_HoldString (progs_t *pr, string_t str)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr = get_strref (res, str);

	if (sr) {
		switch (sr->type) {
			case str_temp:
			case str_return:
				break;
			case str_static:
			case str_mutable:
			case str_dynamic:
				// non-ephemeral string, no-op
				return;
			default:
				PR_Error (pr, "internal string error: %d", __LINE__);
		}
		sr->type = str_dynamic;
		return;
	}
	if (!PR_StringValid (pr, str)) {
		PR_RunError (pr, "attempt to hold invalid string %d", str);
	}
}

VISIBLE void
PR_FreeString (progs_t *pr, string_t str)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr = get_strref (res, str);

	if (sr) {
		switch (sr->type) {
			case str_static:
			case str_temp:
			case str_return:
				return;
			case str_mutable:
				dstring_delete (sr->s.dstring);
				break;
			case str_dynamic:
				pr_strfree (pr, sr->s.string);
				break;
			default:
				PR_Error (pr, "internal string error: %d", __LINE__);
		}
		free_string_ref (res, sr);
		return;
	}
	if (!PR_StringValid (pr, str)) {
		PR_RunError (pr, "attempt to free invalid string %d", str);
	}
}

VISIBLE void
PR_FreeTempStrings (progs_t *pr)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr, *t;

	for (sr = pr->pr_xtstr; sr; sr = t) {
		t = sr->next;
		if (sr->type == str_dynamic) {
			// the string has been held, so simply remove the ref from the
			// queue
			continue;
		}
		if (sr->type != str_temp)
			PR_Error (pr, "internal string error: %d", __LINE__);
		if (R_STRING (pr) < 0 && string_index (res, sr) == R_STRING (pr)
			&& pr->pr_depth) {
			prstack_t  *frame = pr->pr_stack + pr->pr_depth - 1;
			sr->next = frame->tstr;
			frame->tstr = sr;
		} else {
			pr_strfree (pr, sr->s.string);
			free_string_ref (res, sr);
		}
	}
	pr->pr_xtstr = 0;
}

#define PRINT(t)													\
	switch ((doWidth << 1) | doPrecision) {							\
		case 3:														\
			dasprintf (result, tmp->str, current->minFieldWidth,	\
					   current->precision, current->data.t##_var);	\
			break;													\
		case 2:														\
			dasprintf (result, tmp->str, current->minFieldWidth,	\
					   current->data.t##_var);						\
			break;													\
		case 1:														\
			dasprintf (result, tmp->str, current->precision,		\
					   current->data.t##_var);						\
			break;													\
		case 0:														\
			dasprintf (result, tmp->str, current->data.t##_var);	\
			break;													\
	}

/*
	This function takes as input a linked list of fmt_item_t representing
	EVERYTHING to be printed. This includes text that is not affected by
	formatting. A string without any formatting would wind up with only one
	list item.
*/
static void
I_DoPrint (dstring_t *tmp, dstring_t *result, fmt_item_t *formatting)
{
	fmt_item_t		*current = formatting;

	while (current) {
		qboolean	doPrecision, doWidth;

		doPrecision = -1 != current->precision;
		doWidth = 0 != current->minFieldWidth;

		dsprintf (tmp, "%%%s%s%s%s%s%s%s",
			(current->flags & FMT_ALTFORM) ? "#" : "",	// hash
			(current->flags & FMT_ZEROPAD) ? "0" : "",	// zero padding
			(current->flags & FMT_LJUSTIFY) ? "-" : "",	// left justify
			(current->flags & FMT_ADDBLANK) ? " " : "",	// add space for +ve
			(current->flags & FMT_ADDSIGN) ? "+" : "",	// add sign always
			doWidth ? "*" : "",
			doPrecision ? ".*" : "");

		switch (current->type) {
			case 's':
				dstring_appendstr (tmp, "s");
				PRINT (string);
				break;
			case 'c':
				dstring_appendstr (tmp, "c");
				PRINT (integer);
				break;
			case 'i':
			case 'd':
				dstring_appendstr (tmp, "d");
				PRINT (integer);
				break;
			case 'x':
				dstring_appendstr (tmp, "x");
				PRINT (integer);
				break;
			case 'u':
				if (current->flags & FMT_HEX)
					dstring_appendstr (tmp, "x");
				else
					dstring_appendstr (tmp, "u");
				PRINT (uinteger);
				break;
			case 'f':
				dstring_appendstr (tmp, "f");
				if (current->flags & FMT_LONG) {
					PRINT (double);
				} else {
					PRINT (float);
				}
				break;
			case 'g':
				dstring_appendstr (tmp, "g");
				if (current->flags & FMT_LONG) {
					PRINT (double);
				} else {
					PRINT (float);
				}
				break;
			default:
				break;
		}
		current = current->next;
	}
}

static fmt_item_t *
new_fmt_item (prstr_resources_t *res)
{
	int        i;
	fmt_item_t *fi;

	if (!res->free_fmt_items) {
		res->free_fmt_items = malloc (16 * sizeof (fmt_item_t));
		for (i = 0; i < 15; i++)
			res->free_fmt_items[i].next = res->free_fmt_items + i + 1;
		res->free_fmt_items[i].next = 0;
	}

	fi = res->free_fmt_items;
	res->free_fmt_items = fi->next;
	memset (fi, 0, sizeof (*fi));
	fi->precision = -1;
	return fi;
}

static void
free_fmt_item (prstr_resources_t *res, fmt_item_t *fi)
{
	fi->next = res->free_fmt_items;
	res->free_fmt_items = fi;
}

struct fmt_state_s;
typedef void (*fmt_state_f) (struct fmt_state_s *);

typedef struct fmt_state_s {
	fmt_state_f state;
	prstr_resources_t *res;
	progs_t    *pr;
	pr_type_t **args;
	const char *c;
	const char *msg;
	fmt_item_t *fmt_items;
	fmt_item_t **fi;
	int         fmt_count;
} fmt_state_t;

static inline void
fmt_append_item (fmt_state_t *state)
{
	(*state->fi)->next = new_fmt_item (state->res);
	state->fi = &(*state->fi)->next;
}

#undef P_var
#define P_var(p,n,t) (state->args[n]->t##_var)
#undef P_DOUBLE
#define P_DOUBLE(p,n) (*(double *) (state->args[n]))

/**	State machine for PR_Sprintf
 *
 *	Parsing of the format string is implemented via the following state
 *	machine. Note that in all states, end-of-string terminates the machine.
 *	If the machine terminates in any state other than format or conversion,
 *	an error is generated.
 *	\dot
 *	digraph PR_Sprintf_fmt_state_machine {
 *		format          -> flags            [label="{%}"];
 *		flags           -> flags            [label="{#+0 -}"];
 *		flags           -> var_field_width  [label="{*}"];
 *		flags           -> precision        [label="{.}"];
 *		flags           -> field_width      [label="{[1-9]}"];
 *		flags           -> modifiers        [label="other"];
 *		var_field_width -> precision        [label="{.}"];
 *		var_field_width -> modifiers        [label="other"];
 *		field_width     -> field_width      [label="{[0-9]}"];
 *		field_width     -> precision        [label="{.}"];
 *		field_width     -> modifiers        [label="other"];
 *		precision       -> var_precision    [label="{*}"];
 *		precision       -> fixed_precision  [label="{[0-9]}"];
 *		precision       -> modifiers        [label="other"];
 *		var_precision   -> modifiers        [label="instant"];
 *		fixed_precision -> fixed_precision  [label="{[0-9]}"];
 *		fixed_precision -> modifiers        [label="other"];
 *		modifiers       -> conversion       [label="instant/other"];
 *		conversion      -> format           [label="other"];
 *	}
 *	\enddot
 */
///@{
static void fmt_state_format (fmt_state_t *state);
static void fmt_state_flags (fmt_state_t *state);
static void fmt_state_var_field_width (fmt_state_t *state);
static void fmt_state_field_width (fmt_state_t *state);
static void fmt_state_precision (fmt_state_t *state);
static void fmt_state_var_precision (fmt_state_t *state);
static void fmt_state_fixed_precision (fmt_state_t *state);
static void fmt_state_modifiers (fmt_state_t *state);
static void fmt_state_conversion (fmt_state_t *state);

static void
fmt_state_flags (fmt_state_t *state)
{
	state->c++;	// skip over %
	while (1) {
		switch (*state->c) {
			case '0':
				(*state->fi)->flags |= FMT_ZEROPAD;
				break;
			case '#':
				(*state->fi)->flags |= FMT_ALTFORM;
				break;
			case ' ':
				(*state->fi)->flags |= FMT_ADDBLANK;
				break;
			case '-':
				(*state->fi)->flags |= FMT_LJUSTIFY;
				break;
			case '+':
				(*state->fi)->flags |= FMT_ADDSIGN;
				break;
			case '*':
				state->state = fmt_state_var_field_width;
				return;
			case '.':
				state->state = fmt_state_precision;
				return;
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
				state->state = fmt_state_field_width;
				return;
			default:
				state->state = fmt_state_modifiers;
				return;
		}
		state->c++;
	}
}

static void
fmt_state_var_field_width (fmt_state_t *state)
{
	(*state->fi)->minFieldWidth = P_INT (pr, state->fmt_count);
	state->fmt_count++;
	if (*++state->c == '.') {
		state->state = fmt_state_precision;
	} else {
		state->state = fmt_state_modifiers;
	}
}

static void
fmt_state_field_width (fmt_state_t *state)
{
	while (isdigit ((byte )*state->c)) {
		(*state->fi)->minFieldWidth *= 10;
		(*state->fi)->minFieldWidth += *state->c++ - '0';
	}
	if (*state->c == '.') {
		state->state = fmt_state_precision;
	} else {
		state->state = fmt_state_modifiers;
	}
}

static void
fmt_state_precision (fmt_state_t *state)
{
	state->c++;	// skip over .
	(*state->fi)->precision = 0;
	if (isdigit ((byte )*state->c)) {
		state->state = fmt_state_fixed_precision;
	} else if (*state->c == '*') {
		state->state = fmt_state_var_precision;
	} else {
		state->state = fmt_state_modifiers;
	}
}

static void
fmt_state_var_precision (fmt_state_t *state)
{
	state->c++;	// skip over *
	(*state->fi)->precision = P_INT (pr, state->fmt_count);
	state->fmt_count++;
	state->state = fmt_state_modifiers;
}

static void
fmt_state_fixed_precision (fmt_state_t *state)
{
	while (isdigit ((byte )*state->c)) {
		(*state->fi)->precision *= 10;
		(*state->fi)->precision += *state->c++ - '0';
	}
	state->state = fmt_state_modifiers;
}

static void
fmt_state_modifiers (fmt_state_t *state)
{
	// no modifiers supported
	state->state = fmt_state_conversion;
}

static void
fmt_state_conversion (fmt_state_t *state)
{
	progs_t    *pr = state->pr;
	char        conv;
	switch ((conv = *state->c++)) {
		case '@':
			// object
			state->fmt_count++;
			fmt_append_item (state);
			break;
		case 'e':
			// entity
			(*state->fi)->type = 'i';
			(*state->fi)->data.integer_var =
				P_EDICTNUM (pr, state->fmt_count);

			state->fmt_count++;
			fmt_append_item (state);
			break;
		case 'i':
		case 'd':
		case 'c':
			// integer
			(*state->fi)->type = conv;
			(*state->fi)->data.integer_var = P_INT (pr, state->fmt_count);

			state->fmt_count++;
			fmt_append_item (state);
			break;
		case 'f':
			// float or double
		case 'g':
			// float or double, no trailing zeroes, trim "."
			// if nothing after
			(*state->fi)->type = conv;
			if (pr->float_promoted) {
				(*state->fi)->flags |= FMT_LONG;
				(*state->fi)->data.double_var
					= P_DOUBLE (pr, state->fmt_count);
			} else {
				(*state->fi)->data.float_var
					= P_FLOAT (pr, state->fmt_count);
			}

			state->fmt_count++;
			fmt_append_item (state);
			break;
		case 'p':
			// pointer
			(*state->fi)->flags |= FMT_ALTFORM;
			(*state->fi)->type = 'x';
			(*state->fi)->data.uinteger_var = P_UINT (pr, state->fmt_count);

			state->fmt_count++;
			fmt_append_item (state);
			break;
		case 's':
			// string
			(*state->fi)->type = conv;
			(*state->fi)->data.string_var = P_GSTRING (pr, state->fmt_count);

			state->fmt_count++;
			fmt_append_item (state);
			break;
		case 'v':
		case 'q':
			// vector
			{
				int         i, count = 3;
				int         flags = (*state->fi)->flags;
				int         precision = (*state->fi)->precision;
				unsigned    minWidth = (*state->fi)->minFieldWidth;

				(*state->fi)->flags = 0;
				(*state->fi)->precision = -1;
				(*state->fi)->minFieldWidth = 0;

				if (conv == 'q')
					count = 4;

				for (i = 0; i < count; i++) {
					if (i == 0) {
						(*state->fi)->type = 's';
						(*state->fi)->data.string_var = "'";
					} else {
						(*state->fi)->type = 's';
						(*state->fi)->data.string_var = " ";
					}
					fmt_append_item (state);

					(*state->fi)->flags = flags;
					(*state->fi)->precision = precision;
					(*state->fi)->minFieldWidth = minWidth;
					(*state->fi)->type = 'g';
					(*state->fi)->data.float_var =
						P_VECTOR (pr, state->fmt_count)[i];

					fmt_append_item (state);
				}
			}

			(*state->fi)->type = 's';
			(*state->fi)->data.string_var = "'";

			state->fmt_count++;
			fmt_append_item (state);
			break;
		case '%':
			(*state->fi)->flags = 0;
			(*state->fi)->precision = 0;
			(*state->fi)->minFieldWidth = 0;
			(*state->fi)->type = 's';
			(*state->fi)->data.string_var = "%";

			fmt_append_item (state);
			break;
		case 'x':
			// integer, hex notation
			(*state->fi)->type = conv;
			(*state->fi)->data.uinteger_var = P_UINT (pr, state->fmt_count);

			state->fmt_count++;
			fmt_append_item (state);
			break;
	}
	if (*state->c) {
		state->state = fmt_state_format;
	} else {
		state->state = 0;	// finished
	}
}

static void
fmt_state_format (fmt_state_t *state)
{
	const char *l = state->c;
	while (*state->c && *state->c != '%') {
		state->c++;
	}
	if (state->c != l) {
		// have some unformatted text to print
		(*state->fi)->precision = state->c - l;
		(*state->fi)->type = 's';
		(*state->fi)->data.string_var = l;
		if (*state->c) {
			fmt_append_item (state);
		}
	}
	if (*state->c) {
		state->state = fmt_state_flags;
	} else {
		state->state = 0;	// finished
	}
}
///@}

VISIBLE void
PR_Sprintf (progs_t *pr, dstring_t *result, const char *name,
			const char *format, int count, pr_type_t **args)
{
	prstr_resources_t *res = pr->pr_string_resources;
	fmt_state_t state = { };

	state.pr = pr;
	state.res = res;
	state.args = args;
	state.fi = &state.fmt_items;
	*state.fi = new_fmt_item (res);
	state.c = format;
	state.state = fmt_state_format;

	if (!name)
		name = "PR_Sprintf";

	while (*state.c && state.state) {
		state.state (&state);
	}

	if (state.state) {
		state.msg = "Unexpected end of format string";
	} else if (state.fmt_count != count) {
		if (state.fmt_count > count)
			state.msg = "Not enough arguments for format string.";
		else
			state.msg = "Too many arguments for format string.";
	}
	if (state.msg) {
		dsprintf (res->print_str, "%s: %d %d", state.msg, state.fmt_count,
				  count);
		state.msg = res->print_str->str;
		goto error;
	}

	dstring_clear (res->print_str);
	I_DoPrint (res->print_str, result, state.fmt_items);
	while (state.fmt_items) {
		fmt_item_t *t = state.fmt_items->next;
		free_fmt_item (res, state.fmt_items);
		state.fmt_items = t;
	}
	return;
error:
	PR_RunError (pr, "%s: %s", name, state.msg);
}

void
PR_Strings_Init (progs_t *pr)
{
	prstr_resources_t *res = calloc (1, sizeof (*res));
	res->pr = pr;
	res->print_str = dstring_new ();

	PR_Resources_Register (pr, "Strings", res, pr_strings_clear);
	PR_AddLoadFunc (pr, PR_LoadStrings);
}
