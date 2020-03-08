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
										  res);
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

static inline char *
pr_stralloc (progs_t *pr, size_t len)
{
	return PR_Zone_Malloc (pr, len + 1);
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
	// slot is empty
	if ((sr = res->rs_slot->strref)) {
		if (sr->type != str_return || sr->rs_slot != res->rs_slot) {
			PR_Error (pr, "internal string error: %d", __LINE__);
		}
		pr_strfree (pr, sr->s.string);
	} else {
		sr = new_string_ref (res);
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

void
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
PR_FreeString (progs_t *pr, string_t str)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr = get_strref (res, str);

	if (sr) {
		switch (sr->type) {
			case str_static:
			case str_temp:
				return;
			case str_mutable:
				dstring_delete (sr->s.dstring);
				break;
			case str_dynamic:
				pr_strfree (pr, sr->s.string);
				break;
			case str_return:
			default:
				PR_Error (pr, "internal string error: %d", __LINE__);
		}
		free_string_ref (res, sr);
		return;
	}
	PR_RunError (pr, "attempt to free invalid string %d", str);
}

void
PR_FreeTempStrings (progs_t *pr)
{
	prstr_resources_t *res = pr->pr_string_resources;
	strref_t   *sr, *t;

	for (sr = pr->pr_xtstr; sr; sr = t) {
		t = sr->next;
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

#undef P_var
#define P_var(p,n,t) (args[n]->t##_var)
#undef P_DOUBLE
#define P_DOUBLE(p,n) (*(double *) (args[n]))
VISIBLE void
PR_Sprintf (progs_t *pr, dstring_t *result, const char *name,
			const char *format, int count, pr_type_t **args)
{
	prstr_resources_t *res = pr->pr_string_resources;
	const char *c, *l;
	const char *msg = "";
	fmt_item_t *fmt_items = 0;
	fmt_item_t **fi = &fmt_items;
	int         fmt_count = 0;

	if (!name)
		name = "PR_Sprintf";

	*fi = new_fmt_item (res);
	c = l = format;
	while (*c) {
		if (*c++ == '%') {
			if (c != l + 1) {
				// have some unformatted text to print
				(*fi)->precision = c - l - 1;
				(*fi)->type = 's';
				(*fi)->data.string_var = l;

				(*fi)->next = new_fmt_item (res);
				fi = &(*fi)->next;
			}
			if (*c == '%') {
				(*fi)->type = 's';
				(*fi)->data.string_var = "%";

				(*fi)->next = new_fmt_item (res);
				fi = &(*fi)->next;
			} else {
				do {
					switch (*c) {
						// format options
						case '\0':
							msg = "Unexpected end of format string";
							goto error;
						case '0':
							(*fi)->flags |= FMT_ZEROPAD;
							c++;
							continue;
						case '#':
							(*fi)->flags |= FMT_ALTFORM;
							c++;
							continue;
						case ' ':
							(*fi)->flags |= FMT_ADDBLANK;
							c++;
							continue;
						case '-':
							(*fi)->flags |= FMT_LJUSTIFY;
							c++;
							continue;
						case '+':
							(*fi)->flags |= FMT_ADDSIGN;
							c++;
							continue;
						case '.':
							(*fi)->precision = 0;
							c++;
							while (isdigit ((byte )*c)) {
								(*fi)->precision *= 10;
								(*fi)->precision += *c++ - '0';
							}
							continue;
						case '1': case '2': case '3': case '4': case '5':
						case '6': case '7': case '8': case '9':
							while (isdigit ((byte )*c)) {
								(*fi)->minFieldWidth *= 10;
								(*fi)->minFieldWidth += *c++ - '0';
							}
							continue;
						// format types
						case '@':
							// object
							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 'e':
							// entity
							(*fi)->type = 'i';
							(*fi)->data.integer_var =
								P_EDICTNUM (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 'i':
						case 'd':
						case 'c':
							// integer
							(*fi)->type = *c;
							(*fi)->data.integer_var = P_INT (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 'f':
							// float or double
						case 'g':
							// float or double, no trailing zeroes, trim "."
							// if nothing after
							(*fi)->type = *c;
							if (pr->float_promoted) {
								(*fi)->flags |= FMT_LONG;
								(*fi)->data.double_var
									= P_DOUBLE (pr, fmt_count);
							} else {
								(*fi)->data.float_var
									= P_FLOAT (pr, fmt_count);
							}

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 'p':
							// pointer
							(*fi)->flags |= FMT_ALTFORM;
							(*fi)->type = 'x';
							(*fi)->data.uinteger_var = P_UINT (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 's':
							// string
							(*fi)->type = *c;
							(*fi)->data.string_var = P_GSTRING (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 'v':
						case 'q':
							// vector
							{
								int         i, count = 3;
								int         flags = (*fi)->flags;
								int         precision = (*fi)->precision;
								unsigned    minWidth = (*fi)->minFieldWidth;

								(*fi)->flags = 0;
								(*fi)->precision = -1;
								(*fi)->minFieldWidth = 0;

								if (*c == 'q')
									count = 4;

								for (i = 0; i < count; i++) {
									if (i == 0) {
										(*fi)->type = 's';
										(*fi)->data.string_var = "'";
									} else {
										(*fi)->type = 's';
										(*fi)->data.string_var = " ";
									}
									(*fi)->next = new_fmt_item (res);
									fi = &(*fi)->next;

									(*fi)->flags = flags;
									(*fi)->precision = precision;
									(*fi)->minFieldWidth = minWidth;
									(*fi)->type = 'g';
									(*fi)->data.float_var =
										P_VECTOR (pr, fmt_count)[i];

									(*fi)->next = new_fmt_item (res);
									fi = &(*fi)->next;
								}
							}

							(*fi)->type = 's';
							(*fi)->data.string_var = "'";

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
						case 'x':
							// integer, hex notation
							(*fi)->type = *c;
							(*fi)->data.uinteger_var = P_UINT (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item (res);
							fi = &(*fi)->next;
							break;
					}
					break;
				} while (1);
			}
			l = ++c;
		}
	}
	if (c != l) {
		// have some unformatted text to print
		(*fi)->precision = c - l;
		(*fi)->type = 's';
		(*fi)->data.string_var = l;

		(*fi)->next = new_fmt_item (res);
	}

	if (fmt_count != count) {
		if (fmt_count > count)
			msg = "Not enough arguments for format string.";
		else
			msg = "Too many arguments for format string.";
		dsprintf (res->print_str, "%s: %d %d", msg, fmt_count, count);
		msg = res->print_str->str;
		goto error;
	}

	dstring_clear (res->print_str);
	I_DoPrint (res->print_str, result, fmt_items);
	while (fmt_items) {
		fmt_item_t *t = fmt_items->next;
		free_fmt_item (res, fmt_items);
		fmt_items = t;
	}
	return;
error:
	PR_RunError (pr, "%s: %s", name, msg);
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
