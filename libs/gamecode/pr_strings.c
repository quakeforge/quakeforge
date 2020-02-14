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
#include "QF/va.h"

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
	strref_t  **prev;
	str_e       type;
	union {
		char       *string;
		dstring_t  *dstring;
	} s;
};

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

static fmt_item_t *free_fmt_items;

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
new_string_ref (progs_t *pr)
{
	strref_t *sr;
	if (!pr->free_string_refs) {
		int		    i;
		size_t      size;

		pr->dyn_str_size++;
		size = pr->dyn_str_size * sizeof (strref_t *);
		pr->string_map = realloc (pr->string_map, size);
		if (!pr->string_map)
			PR_Error (pr, "out of memory");
		if (!(pr->free_string_refs = calloc (1024, sizeof (strref_t))))
			PR_Error (pr, "out of memory");
		pr->string_map[pr->dyn_str_size - 1] = pr->free_string_refs;
		for (i = 0, sr = pr->free_string_refs; i < 1023; i++, sr++)
			sr->next = sr + 1;
		sr->next = 0;
	}
	sr = pr->free_string_refs;
	pr->free_string_refs = sr->next;
	sr->next = 0;
	return sr;
}

static void
free_string_ref (progs_t *pr, strref_t *sr)
{
	sr->type = str_free;
	if (sr->prev)
		*sr->prev = sr->next;
	sr->next = pr->free_string_refs;
	pr->free_string_refs = sr;
}

static __attribute__((pure)) string_t
string_index (progs_t *pr, strref_t *sr)
{
	long        o = (long) (sr - pr->static_strings);
	unsigned    i;

	if (o >= 0 && o < pr->num_strings)
		return sr->s.string - pr->pr_strings;
	for (i = 0; i < pr->dyn_str_size; i++) {
		int         d = sr - pr->string_map[i];
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
strref_free (void *_sr, void *_pr)
{
	progs_t		*pr = (progs_t*)_pr;
	strref_t	*sr = (strref_t*)_sr;

	// Since this is called only by Hash_FlushTable, the memory pointed
	// to by sr->string or sr->dstring has already been lost in the progs
	// load/reload and thus there's no need to free it.

	// free the string and ref only if it's not a static string
	if (sr < pr->static_strings || sr >= pr->static_strings + pr->num_strings) {
		free_string_ref (pr, sr);
	}
}

VISIBLE int
PR_LoadStrings (progs_t *pr)
{
	char   *end = pr->pr_strings + pr->progs->numstrings;
	char   *str = pr->pr_strings;
	int		count = 0;

	while (str < end) {
		count++;
		str += strlen (str) + 1;
	}

	if (!pr->ds_mem) {
		pr->ds_mem = malloc (sizeof (dstring_mem_t));
		pr->ds_mem->alloc = pr_strings_alloc;
		pr->ds_mem->free = pr_strings_free;
		pr->ds_mem->realloc = pr_strings_realloc;
		pr->ds_mem->data = pr;
	}
	if (pr->strref_hash) {
		Hash_FlushTable (pr->strref_hash);
	} else {
		pr->strref_hash = Hash_NewTable (1021, strref_get_key, strref_free,
										 pr);
		pr->string_map = 0;
		pr->free_string_refs = 0;
		pr->dyn_str_size = 0;
	}

	if (pr->static_strings)
		free (pr->static_strings);
	pr->static_strings = malloc (count * sizeof (strref_t));
	count = 0;
	str = pr->pr_strings;
	while (str < end) {
		if (!Hash_Find (pr->strref_hash, str)) {
			pr->static_strings[count].type = str_static;
			pr->static_strings[count].s.string = str;
			Hash_Add (pr->strref_hash, &pr->static_strings[count]);
			count++;
		}
		str += strlen (str) + 1;
	}
	pr->num_strings = count;
	return 1;
}

static inline strref_t *
get_strref (progs_t *pr, string_t num)
{
	if (num < 0) {
		strref_t   *ref;
		unsigned    row = ~num / 1024;

		num = ~num % 1024;

		if (row >= pr->dyn_str_size)
			return 0;
		ref = &pr->string_map[row][num];
		if (ref->type == str_free)
			return 0;
		return ref;
	}
	return 0;
}

static inline __attribute__((pure)) const char *
get_string (progs_t *pr, string_t num)
{
	if (num < 0) {
		strref_t   *ref = get_strref (pr, num);
		if (!ref)
			return 0;
		switch (ref->type) {
			case str_static:
			case str_temp:
			case str_dynamic:
			case str_return:
				return ref->s.string;
			case str_mutable:
				return ref->s.dstring->str;
			case str_free:
				break;
		}
		PR_Error (pr, "internal string error");
	} else {
		if (num >= pr->pr_stringsize)
			return 0;
		return pr->pr_strings + num;
	}
}

VISIBLE qboolean
PR_StringValid (progs_t *pr, string_t num)
{
	return get_string (pr, num) != 0;
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
	strref_t   *ref = get_strref (pr, num);
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
	strref_t   *sr;

	if (!s)
		s = "";
	sr = Hash_Find (pr->strref_hash, s);

	if (__builtin_expect (!sr, 1)) {
		sr = new_string_ref (pr);
		sr->type = str_static;
		sr->s.string = pr_strdup(pr, s);
		Hash_Add (pr->strref_hash, sr);
	}
	return string_index (pr, sr);
}

void
PR_ClearReturnStrings (progs_t *pr)
{
	int         i;

	for (i = 0; i < PR_RS_SLOTS; i++) {
		if (pr->return_strings[i])
			free_string_ref (pr, pr->return_strings[i]);
		pr->return_strings[i] = 0;
	}
}

VISIBLE string_t
PR_SetReturnString (progs_t *pr, const char *s)
{
	strref_t   *sr;

	if (!s)
		s = "";
	if ((sr = Hash_Find (pr->strref_hash, s))) {
		return string_index (pr, sr);
	}

	if ((sr = pr->return_strings[pr->rs_slot])) {
		if (sr->type != str_return)
			PR_Error (pr, "internal string error");
		pr_strfree (pr, sr->s.string);
	} else {
		sr = new_string_ref (pr);
	}
	sr->type = str_return;
	sr->s.string = pr_strdup(pr, s);

	pr->return_strings[pr->rs_slot++] = sr;
	pr->rs_slot %= PR_RS_SLOTS;
	return string_index (pr, sr);
}

static inline string_t
pr_settempstring (progs_t *pr, char *s)
{
	strref_t   *sr;

	sr = new_string_ref (pr);
	sr->type = str_temp;
	sr->s.string = s;
	sr->next = pr->pr_xtstr;
	pr->pr_xtstr = sr;
	return string_index (pr, sr);
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

	return pr_settempstring (pr, c);
}

VISIBLE string_t
PR_SetTempString (progs_t *pr, const char *s)
{
	strref_t   *sr;

	if (!s)
		return PR_SetString (pr, "");

	if ((sr = Hash_Find (pr->strref_hash, s))) {
		return string_index (pr, sr);
	}

	return pr_settempstring (pr, pr_strdup (pr, s));
}

VISIBLE string_t
PR_SetDynamicString (progs_t *pr, const char *s)
{
	strref_t   *sr;

	if (!s)
		return PR_SetString (pr, "");

	if ((sr = Hash_Find (pr->strref_hash, s))) {
		return string_index (pr, sr);
	}

	sr = new_string_ref (pr);
	sr->type = str_dynamic;
	sr->s.string = pr_strdup (pr, s);
	return string_index (pr, sr);
}

void
PR_MakeTempString (progs_t *pr, string_t str)
{
	strref_t   *sr = get_strref (pr, str);

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
	strref_t   *sr = new_string_ref (pr);
	sr->type = str_mutable;
	sr->s.dstring = _dstring_newstr (pr->ds_mem);
	return string_index (pr, sr);
}

VISIBLE void
PR_FreeString (progs_t *pr, string_t str)
{
	strref_t   *sr = get_strref (pr, str);

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
				PR_Error (pr, "internal string error");
		}
		free_string_ref (pr, sr);
		return;
	}
	if (!get_string (pr, str))
		PR_RunError (pr, "attempt to free invalid string %d", str);
}

void
PR_FreeTempStrings (progs_t *pr)
{
	strref_t   *sr, *t;

	for (sr = pr->pr_xtstr; sr; sr = t) {
		t = sr->next;
		if (sr->type != str_temp)
			PR_Error (pr, "internal string error");
		if (R_STRING (pr) < 0 && string_index (pr, sr) == R_STRING (pr)
			&& pr->pr_depth) {
			prstack_t  *frame = pr->pr_stack + pr->pr_depth - 1;
			sr->next = frame->tstr;
			frame->tstr = sr;
		} else {
			pr_strfree (pr, sr->s.string);
			free_string_ref (pr, sr);
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
I_DoPrint (dstring_t *result, fmt_item_t *formatting)
{
	fmt_item_t		*current = formatting;
	dstring_t		*tmp = dstring_new ();

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
	dstring_delete (tmp);
}

static fmt_item_t *
new_fmt_item (void)
{
	int        i;
	fmt_item_t *fi;

	if (!free_fmt_items) {
		free_fmt_items = malloc (16 * sizeof (fmt_item_t));
		for (i = 0; i < 15; i++)
			free_fmt_items[i].next = free_fmt_items + i + 1;
		free_fmt_items[i].next = 0;
	}

	fi = free_fmt_items;
	free_fmt_items = fi->next;
	memset (fi, 0, sizeof (*fi));
	fi->precision = -1;
	return fi;
}

static void
free_fmt_item (fmt_item_t *fi)
{
	fi->next = free_fmt_items;
	free_fmt_items = fi;
}

#undef P_var
#define P_var(p,n,t) (args[n]->t##_var)
VISIBLE void
PR_Sprintf (progs_t *pr, dstring_t *result, const char *name,
			const char *format, int count, pr_type_t **args)
{
	const char *c, *l;
	const char *msg = "";
	fmt_item_t *fmt_items = 0;
	fmt_item_t **fi = &fmt_items;
	int         fmt_count = 0;

	if (!name)
		name = "PR_Sprintf";

	*fi = new_fmt_item ();
	c = l = format;
	while (*c) {
		if (*c++ == '%') {
			if (c != l + 1) {
				// have some unformatted text to print
				(*fi)->precision = c - l - 1;
				(*fi)->type = 's';
				(*fi)->data.string_var = l;

				(*fi)->next = new_fmt_item ();
				fi = &(*fi)->next;
			}
			if (*c == '%') {
				(*fi)->type = 's';
				(*fi)->data.string_var = "%";

				(*fi)->next = new_fmt_item ();
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
							(*fi)->next = new_fmt_item ();
							fi = &(*fi)->next;
							break;
						case 'e':
							// entity
							(*fi)->type = 'i';
							(*fi)->data.integer_var =
								P_EDICTNUM (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item ();
							fi = &(*fi)->next;
							break;
						case 'i':
						case 'd':
						case 'c':
							// integer
							(*fi)->type = *c;
							(*fi)->data.integer_var = P_INT (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item ();
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
							(*fi)->next = new_fmt_item ();
							fi = &(*fi)->next;
							break;
						case 'p':
							// pointer
							(*fi)->flags |= FMT_ALTFORM;
							(*fi)->type = 'x';
							(*fi)->data.uinteger_var = P_UINT (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item ();
							fi = &(*fi)->next;
							break;
						case 's':
							// string
							(*fi)->type = *c;
							(*fi)->data.string_var = P_GSTRING (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item ();
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
									(*fi)->next = new_fmt_item ();
									fi = &(*fi)->next;

									(*fi)->flags = flags;
									(*fi)->precision = precision;
									(*fi)->minFieldWidth = minWidth;
									(*fi)->type = 'g';
									(*fi)->data.float_var =
										P_VECTOR (pr, fmt_count)[i];

									(*fi)->next = new_fmt_item ();
									fi = &(*fi)->next;
								}
							}

							(*fi)->type = 's';
							(*fi)->data.string_var = "'";

							fmt_count++;
							(*fi)->next = new_fmt_item ();
							fi = &(*fi)->next;
							break;
						case 'x':
							// integer, hex notation
							(*fi)->type = *c;
							(*fi)->data.uinteger_var = P_UINT (pr, fmt_count);

							fmt_count++;
							(*fi)->next = new_fmt_item ();
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

		(*fi)->next = new_fmt_item ();
	}

	if (fmt_count != count) {
		if (fmt_count > count)
			msg = "Not enough arguments for format string.";
		else
			msg = "Too many arguments for format string.";
		msg = va ("%s: %d %d", msg, fmt_count, count);
		goto error;
	}

	I_DoPrint (result, fmt_items);
	while (fmt_items) {
		fmt_item_t *t = fmt_items->next;
		free_fmt_item (fmt_items);
		fmt_items = t;
	}
	return;
error:
	PR_RunError (pr, "%s: %s", name, msg);
}
