/*
	qfplist.c

	Property list management

	Copyright (C) 2000  Jeff Teunissen <deek@d2dc.net>

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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "qfalloca.h"

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qfplist.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

/*
	Generic property list item.
*/
struct plitem_s {
	pltype_t	type;
	int         line;
	void		*data;
};

/*
	Dictionaries
*/
struct dictkey_s {
	char		*key;
	plitem_t	*value;
};
typedef struct dictkey_s	dictkey_t;

/*
	Arrays
*/
struct plarray_s {
	int				numvals;		///< Number of items in array
	int				maxvals;		///< Number of items that can be stored
									///< before a realloc is necesary.
	struct plitem_s **values;		///< Array data
};
typedef struct plarray_s	plarray_t;

/*
	Typeless, unformatted binary data
*/
struct plbinary_s {
	size_t		size;
	void		*data;
};
typedef struct plbinary_s	plbinary_t;

typedef struct pldata_s {	// Unparsed property list string
	const char		*ptr;
	unsigned int	end;
	unsigned int	pos;
	unsigned int	line;
	const char		*error;
} pldata_t;

//	Ugly defines for fast checking and conversion from char to number
#define inrange(ch,min,max) ((ch) >= (min) && (ch) <= (max))
#define char2num(ch) \
	(inrange((ch), '0', '9') ? ((ch) - '0') \
							 : 10 + (inrange((ch), 'a', 'f') ? ((ch) - 'a') \
															 : ((ch) - 'A')))

static byte quotable_bitmap[32];
static inline int
is_quotable (byte x)
{
	return quotable_bitmap[x / 8] & (1 << (x % 8));
}

static void
init_quotables (void)
{
	const char *unquotables = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
							  "abcdefghijklmnopqrstuvwxyz!#$%&*+-./:?@|~_^";
	const byte *c;
	memset (quotable_bitmap, ~0, sizeof (quotable_bitmap));
	for (c = (byte *) unquotables; *c; c++)
		quotable_bitmap[*c / 8] &= ~(1 << (*c % 8));
}

static plitem_t *PL_ParsePropertyListItem (pldata_t *);
static qboolean PL_SkipSpace (pldata_t *);
static char *PL_ParseQuotedString (pldata_t *);
static char *PL_ParseUnquotedString (pldata_t *);
static char *PL_ParseData (pldata_t *, int *);

static const char *
dict_get_key (const void *i, void *unused)
{
	dictkey_t	*item = (dictkey_t *) i;
	return item->key;
}

static void
dict_free (void *i, void *unused)
{
	dictkey_t	*item = (dictkey_t *) i;
	free (item->key);
	if (item->value)		// Make descended stuff get freed
		PL_Free (item->value);
	free (item);
}

static plitem_t *
PL_NewItem (pltype_t type)
{
	plitem_t   *item = calloc (1, sizeof (plitem_t));
	item->type = type;
	return item;
}

VISIBLE plitem_t *
PL_NewDictionary (void)
{
	plitem_t   *item = PL_NewItem (QFDictionary);
	//FIXME need a per-thread hashlink freelist for plist to be thread-safe
	hashtab_t  *dict = Hash_NewTable (1021, dict_get_key, dict_free, NULL, 0);
	item->data = dict;
	return item;
}

VISIBLE plitem_t *
PL_NewArray (void)
{
	plitem_t   *item = PL_NewItem (QFArray);
	plarray_t  *array = calloc (1, sizeof (plarray_t));
	item->data = array;
	return item;
}

VISIBLE plitem_t *
PL_NewData (void *data, size_t size)
{
	plitem_t   *item = PL_NewItem (QFBinary);
	plbinary_t *bin = malloc (sizeof (plbinary_t));
	item->data = bin;
	bin->data = data;
	bin->size = size;
	return item;
}

static plitem_t *
new_string (char *str, int line)
{
	plitem_t   *item = PL_NewItem (QFString);
	item->data = str;
	item->line = line;
	return item;
}

VISIBLE plitem_t *
PL_NewString (const char *str)
{
	return new_string (strdup (str), 0);
}

VISIBLE void
PL_Free (plitem_t *item)
{
	switch (item->type) {
		case QFDictionary:
			Hash_DelTable (item->data);
			break;

		case QFArray: {
				int 	i = ((plarray_t *) item->data)->numvals;

				while (i-- > 0) {
					PL_Free (((plarray_t *) item->data)->values[i]);
				}
				free (((plarray_t *) item->data)->values);
				free (item->data);
			}
			break;

		case QFBinary:
			free (((plbinary_t *) item->data)->data);
			free (item->data);
			break;

		case QFString:
			free (item->data);
			break;
	}
	free (item);
}

VISIBLE const char *
PL_String (plitem_t *string)
{
	if (string->type != QFString)
		return NULL;
	return string->data;
}

VISIBLE plitem_t *
PL_ObjectForKey (plitem_t *dict, const char *key)
{
	hashtab_t  *table = (hashtab_t *) dict->data;
	dictkey_t  *k;

	if (dict->type != QFDictionary)
		return NULL;

	k = (dictkey_t *) Hash_Find (table, key);
	return k ? k->value : NULL;
}

VISIBLE plitem_t *
PL_RemoveObjectForKey (plitem_t *dict, const char *key)
{
	hashtab_t  *table = (hashtab_t *) dict->data;
	dictkey_t  *k;
	plitem_t   *value;

	if (dict->type != QFDictionary)
		return NULL;

	k = (dictkey_t *) Hash_Del (table, key);
	if (!k)
		return NULL;
	value = k->value;
	k->value = 0;
	dict_free (k, 0);
	return value;
}

VISIBLE plitem_t *
PL_D_AllKeys (plitem_t *dict)
{
	void		**list, **l;
	dictkey_t	*current;
	plitem_t	*array;

	if (dict->type != QFDictionary)
		return NULL;

	if (!(l = list = Hash_GetList ((hashtab_t *) dict->data)))
		return NULL;

	if (!(array = PL_NewArray ()))
		return NULL;

	while ((current = (dictkey_t *) *l++)) {
		PL_A_AddObject (array, PL_NewString (current->key));
	}
	free (list);

	return array;
}

VISIBLE int
PL_D_NumKeys (plitem_t *dict)
{
	if (dict->type != QFDictionary)
		return 0;
	return Hash_NumElements ((hashtab_t *) dict->data);
}

VISIBLE plitem_t *
PL_ObjectAtIndex (plitem_t *array, int index)
{
	plarray_t  *arr = (plarray_t *) array->data;

	if (array->type != QFArray)
		return NULL;

	return index >= 0 && index < arr->numvals ? arr->values[index] : NULL;
}

VISIBLE qboolean
PL_D_AddObject (plitem_t *dict, const char *key, plitem_t *value)
{
	dictkey_t	*k;

	if (dict->type != QFDictionary)
		return false;

	if ((k = Hash_Find ((hashtab_t *)dict->data, key))) {
		PL_Free ((plitem_t *) k->value);
		k->value = value;
	} else {
		k = malloc (sizeof (dictkey_t));

		if (!k)
			return false;

		k->key = strdup (key);
		k->value = value;

		Hash_Add ((hashtab_t *)dict->data, k);
	}
	return true;
}

VISIBLE qboolean
PL_A_InsertObjectAtIndex (plitem_t *array, plitem_t *item, int index)
{
	plarray_t  *arr;

	if (array->type != QFArray)
		return false;

	arr = (plarray_t *)array->data;

	if (arr->numvals == arr->maxvals) {
		int         size = (arr->maxvals + 128) * sizeof (plitem_t *);
		plitem_t  **tmp = realloc (arr->values, size);

		if (!tmp)
			return false;

		arr->maxvals += 128;
		arr->values = tmp;
		memset (arr->values + arr->numvals, 0,
				(arr->maxvals - arr->numvals) * sizeof (plitem_t *));
	}

	if (index == -1)
		index = arr->numvals;

	if (index < 0 || index > arr->numvals)
		return false;

	memmove (arr->values + index + 1, arr->values + index,
			 (arr->numvals - index) * sizeof (plitem_t *));
	arr->values[index] = item;
	arr->numvals++;
	return true;
}

VISIBLE qboolean
PL_A_AddObject (plitem_t *array, plitem_t *item)
{
	return PL_A_InsertObjectAtIndex (array, item, -1);
}

VISIBLE int
PL_A_NumObjects (plitem_t *array)
{
	if (array->type != QFArray)
		return 0;
	return ((plarray_t *) array->data)->numvals;
}

VISIBLE plitem_t *
PL_RemoveObjectAtIndex (plitem_t *array, int index)
{
	plarray_t  *arr;
	plitem_t   *item;

	if (array->type != QFArray)
		return 0;

	arr = (plarray_t *)array->data;

	if (index < 0 || index >= arr->numvals)
		return 0;

	item = arr->values[index];
	arr->numvals--;
	while (index < arr->numvals) {
		arr->values[index] = arr->values[index + 1];
		index++;
	}

	return item;
}

static qboolean
PL_SkipSpace (pldata_t *pl)
{
	while (pl->pos < pl->end) {
		char	c = pl->ptr[pl->pos];

		if (!isspace ((byte) c)) {
			if (c == '/' && pl->pos < pl->end - 1) {	// check for comments
				if (pl->ptr[pl->pos + 1] == '/') {
					pl->pos += 2;

					while (pl->pos < pl->end) {
						c = pl->ptr[pl->pos];

						if (c == '\n')
							break;
						pl->pos++;
					}
					if (pl->pos >= pl->end) {
						pl->error = "Reached end of string in comment";
						return false;
					}
				} else if (pl->ptr[pl->pos + 1] == '*') {	// "/*" comments
					pl->pos += 2;

					while (pl->pos < pl->end) {
						c = pl->ptr[pl->pos];

						if (c == '\n') {
							pl->line++;
						} else if (c == '*' && pl->pos < pl->end - 1
									&& pl->ptr[pl->pos+1] == '/') {
							pl->pos++;
							break;
						}
						pl->pos++;
					}
					if (pl->pos >= pl->end) {
						pl->error = "Reached end of string in comment";
						return false;
					}
				} else {
					return true;
				}
			} else {
				return true;
			}
		}
		if (c == '\n') {
			pl->line++;
		}
		pl->pos++;
	}
	pl->error = "Reached end of string";
	return false;
}

static inline byte
from_hex (byte a)
{
	if (a >= '0' && a <= '9')
		return a - '0';
	if (a >= 'A' && a <= 'F')
		return a - 'A' + 10;
	return a - 'a' + 10;
}

static inline byte
make_byte (byte h, byte l)
{
	return (from_hex (h) << 4) | from_hex (l);
}

static char *
PL_ParseData (pldata_t *pl, int *len)
{
	unsigned    start = ++pl->pos;
	int         nibbles = 0, i;
	char       *str, c;

	while (pl->pos < pl->end) {
		c = pl->ptr[pl->pos++];
		if (isxdigit ((byte) c)) {
			nibbles++;
			continue;
		}
		if (c == '>') {
			if (nibbles & 1) {
				pl->error = "invalid data, missing nibble";
				return NULL;
			}
			*len = nibbles / 2;
			str = malloc (*len);
			for (i = 0; i < *len; i++)
				str[i] = make_byte (pl->ptr[start + i * 2],
									pl->ptr[start + i * 2 + 1]);
			return str;
		}
		pl->error = "invalid character in data";
		return NULL;
	}
	pl->error = "Reached end of string while parsing data";
	return NULL;
}

static char *
PL_ParseQuotedString (pldata_t *pl)
{
	unsigned int	start = ++pl->pos;
	unsigned int	escaped = 0;
	unsigned int	shrink = 0;
	qboolean		hex = false;
	qboolean        long_string = false;
	char			*str;

	if (pl->ptr[pl->pos] == '"' &&
		pl->ptr[pl->pos + 1] == '"') {
		long_string = true;
		start += 2;
		pl->pos += 2;
	}

	while (pl->pos < pl->end) {

		char	c = pl->ptr[pl->pos];

		if (escaped) {
			if (escaped == 1 && inrange (c, '0', '7')) {
				escaped++;
				hex = false;
			} else if (escaped > 1) {
				if (escaped == 2 && c == 'x') {
					hex = true;
					shrink++;
					escaped++;
				} else if (hex && inrange (escaped, 3, 4)
						   && isxdigit ((byte) c)) {
					shrink++;
					escaped++;
				} else if (inrange (escaped, 1, 3) && inrange (c, '0', '7')) {
					shrink++;
					escaped++;
				} else {
					pl->pos--;
					escaped = 0;
				}
			} else {
				escaped = 0;
			}
		} else {
			if (c == '\\') {
				escaped = 1;
				shrink++;
			} else if (c == '"'
					   && (!long_string || (pl->ptr[pl->pos + 1] == '"'
											&& pl->ptr[pl->pos + 2] == '"'))) {
				break;
			}
		}

		if (c == '\n') {
			pl->line++;
		}

		pl->pos++;
	}

	if (pl->pos >= pl->end) {
		pl->error = "Reached end of string while parsing quoted string";
		return NULL;
	}

	if (pl->pos - start - shrink == 0) {
		str = strdup ("");
	} else {
		char			*chars = alloca(pl->pos - start - shrink);
		unsigned int	j;
		unsigned int	k;

		escaped = 0;
		hex = false;

		for (j = start, k = 0; j < pl->pos; j++) {

			char	c = pl->ptr[j];

			if (escaped) {
				if (escaped == 1 && inrange (c, '0', '7')) {
					chars[k] = c - '0';
					hex = false;
					escaped++;
				} else if (escaped > 1) {
					if (escaped == 2 && c == 'x') {
						hex = true;
						escaped++;
					} else if (hex && inrange (escaped, 3, 4)
							   && isxdigit ((byte) c)) {
						chars[k] <<= 4;
						chars[k] |= char2num (c);
						escaped++;
					} else if (inrange (escaped, 1, 3)
							   && inrange (c, '0', '7')) {
						chars[k] <<= 3;
						chars[k] |= (c - '0');
						escaped++;
					} else {
						escaped = 0;
						j--;
						k++;
					}
				} else {
					escaped = 0;
					switch (c) {
						case 'a':
							chars[k] = '\a';
							break;
						case 'b':
							chars[k] = '\b';
							break;
						case 't':
							chars[k] = '\t';
							break;
						case 'r':
							chars[k] = '\r';
							break;
						case 'n':
							chars[k] = '\n';
							break;
						case 'v':
							chars[k] = '\v';
							break;
						case 'f':
							chars[k] = '\f';
							break;
						default:
							chars[k] = c;
							break;
					}
					k++;
				}
			} else {
				chars[k] = c;
				if (c == '\\') {
					escaped = 1;
				} else {
					k++;
				}
			}
		}
		str = strncat (calloc ((pl->pos - start - shrink) + 1, 1), chars,
					   pl->pos - start - shrink);
	}
	if (long_string)
		pl->pos += 2;
	pl->pos++;
	return str;
}

static char *
PL_ParseUnquotedString (pldata_t *pl)
{
	unsigned int	start = pl->pos;
	char			*str;

	while (pl->pos < pl->end) {
		if (is_quotable (pl->ptr[pl->pos]))
			break;
		pl->pos++;
	}
	str = strncat (calloc ((pl->pos - start) + 1, 1), &pl->ptr[start],
				   pl->pos - start);
	return str;
}

static plitem_t *
PL_ParsePropertyListItem (pldata_t *pl)
{
	plitem_t	*item = NULL;

	if (!PL_SkipSpace (pl))
		return NULL;

	switch (pl->ptr[pl->pos]) {
	case '{':
	{
		item = PL_NewDictionary ();
		item->line = pl->line;

		pl->pos++;

		while (PL_SkipSpace (pl) && pl->ptr[pl->pos] != '}') {
			plitem_t	*key;
			plitem_t	*value;

			if (!(key = PL_ParsePropertyListItem (pl))) {
				PL_Free (item);
				return NULL;
			}

			if (!(PL_SkipSpace (pl))) {
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}

			if (key->type != QFString) {
				pl->error = "Key is not a string";
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}

			if (pl->ptr[pl->pos] != '=') {
				pl->error = "Unexpected character (expected '=')";
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}
			pl->pos++;

			// If there is no value, lose the key
			if (!(value = PL_ParsePropertyListItem (pl))) {
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}

			if (!(PL_SkipSpace (pl))) {
				PL_Free (key);
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			if (pl->ptr[pl->pos] == ';') {
				pl->pos++;
			} else if (pl->ptr[pl->pos] != '}') {
				pl->error = "Unexpected character (wanted ';' or '}')";
				PL_Free (key);
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			// Add the key/value pair to the dictionary
			if (!PL_D_AddObject (item, PL_String (key), value)) {
				PL_Free (key);
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}
			PL_Free (key);
		}

		if (pl->pos >= pl->end) {	// Catch the error
			pl->error = "Unexpected end of string when parsing dictionary";
			PL_Free (item);
			return NULL;
		}
		pl->pos++;

		return item;
	}

	case '(': {
		item = PL_NewArray ();
		item->line = pl->line;

		pl->pos++;

		while (PL_SkipSpace (pl) && pl->ptr[pl->pos] != ')') {
			plitem_t	*value;

			if (!(value = PL_ParsePropertyListItem (pl))) {
				PL_Free (item);
				return NULL;
			}

			if (!(PL_SkipSpace (pl))) {
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			if (pl->ptr[pl->pos] == ',') {
				pl->pos++;
			} else if (pl->ptr[pl->pos] != ')') {
				pl->error = "Unexpected character (wanted ',' or ')')";
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			if (!PL_A_AddObject (item, value)) {
				pl->error = "Unexpected character (too many items in array)";
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}
		}
		pl->pos++;

		return item;
	}

	case '<': {
		int len;
		char *str = PL_ParseData (pl, &len);

		if (!str) {
			return NULL;
		} else {
			item = PL_NewData (str, len);
			item->line = pl->line;
			return item;
		}
	}

	case '"': {
		int line = pl->line;
		char *str = PL_ParseQuotedString (pl);

		if (!str) {
			return NULL;
		} else {
			return new_string (str, line);
		}
	}

	default: {
		int line = pl->line;
		char *str = PL_ParseUnquotedString (pl);

		if (!str) {
			return NULL;
		} else {
			return new_string (str, line);
		}
	}
	} // switch
}

VISIBLE plitem_t *
PL_GetPropertyList (const char *string)
{
	pldata_t	*pl = calloc (1, sizeof (pldata_t));
	plitem_t	*newpl = NULL;

	if (!quotable_bitmap[0])
		init_quotables ();

	pl->ptr = string;
	pl->pos = 0;
	pl->end = strlen (string);
	pl->error = NULL;
	pl->line = 1;

	if ((newpl = PL_ParsePropertyListItem (pl))) {
		free (pl);
		return newpl;
	} else {
		if (pl && pl->error && pl->error[0])
			Sys_Printf ("plist: %d,%d: %s", pl->line, pl->pos, pl->error);
		free (pl);
		return NULL;
	}
}

static void
write_tabs (dstring_t *dstr, int num)
{
	char       *tabs = dstring_reservestr (dstr, num);

	memset (tabs, '\t', num);
	tabs[num] = 0;
}

static void
write_string_len (dstring_t *dstr, const char *str, int len)
{
	char       *dst = dstring_reservestr (dstr, len);
	memcpy (dst, str, len);
	dst[len] = 0;
}

static char
to_hex (byte b)
{
	char        c = (b & 0xf) + '0';
	if (c > '9')
		c = c - '0' + 'A';
	return c;
}

static void
write_binary (dstring_t *dstr, byte *binary, int len)
{
	int         i;
	char       *dst = dstring_reservestr (dstr, len * 2);
	for (i = 0; i < len; i++) {
		*dst++ = to_hex (binary[i] >> 4);
		*dst++ = to_hex (binary[i]);
	}
}

static void
write_string (dstring_t *dstr, const char *str)
{
	const char *s;
	int         len = 0;
	char       *dst;
	int         quoted = 0;

	for (s = str; *s; s++) {
		if (is_quotable (*s))
			quoted = 1;
		len++;
	}
	if (!quoted) {
		dst = dstring_reservestr (dstr, len);
		strcpy (dst, str);
		return;
	}
	// assume worst case of all octal chars plus two quotes.
	dst = dstring_reservestr (dstr, len * 4 + 2);
	*dst++= '\"';
	while (*str) {
		if (*str && isascii ((byte) *str) && isprint ((byte) *str)
			&& *str != '\\' && *str != '\"') {
			*dst++ = *str++;
			continue;
		}
		if (*str) {
			*dst++ = '\\';
			switch (*str) {
				case '\"':
				case '\\':
					*dst++ = *str;
					break;
				case '\n':
					*dst++ = 'n';
					break;
				case '\a':
					*dst++ = 'a';
					break;
				case '\b':
					*dst++ = 'b';
					break;
				case '\f':
					*dst++ = 'f';
					break;
				case '\r':
					*dst++ = 'r';
					break;
				case '\t':
					*dst++ = 't';
					break;
				case '\v':
					*dst++ = 'v';
					break;
				default:
					*dst++ = '0' + ((((byte) *str) >> 6) & 3);
					*dst++ = '0' + ((((byte) *str) >> 3) & 7);
					*dst++ = '0' + (((byte) *str) & 7);
					break;
			}
			str++;
		}
	}
	*dst++ = '\"';
	*dst++ = 0;
	dstr->size = dst - dstr->str;
}

static void
write_item (dstring_t *dstr, plitem_t *item, int level)
{
	void		**list, **l;
	dictkey_t	*current;
	plarray_t	*array;
	plbinary_t	*binary;
	int			i;

	switch (item->type) {
		case QFDictionary:
			write_string_len (dstr, "{\n", 2);
			l = list = Hash_GetList ((hashtab_t *) item->data);
			while ((current = (dictkey_t *) *l++)) {
				write_tabs (dstr, level + 1);
				write_string (dstr, current->key);
				write_string_len (dstr, " = ", 3);
				write_item (dstr, current->value, level + 1);
				write_string_len (dstr, ";\n", 2);
			}
			free (list);
			write_tabs (dstr, level);
			write_string_len (dstr, "}", 1);
			break;
		case QFArray:
			write_string_len (dstr, "(\n", 2);
			array = (plarray_t *) item->data;
			for (i = 0; i < array->numvals; i++) {
				write_tabs (dstr, level + 1);
				write_item (dstr, array->values[i], level + 1);
				if (i < array->numvals - 1)
					write_string_len (dstr, ",\n", 2);
			}
			write_string_len (dstr, "\n", 1);
			write_tabs (dstr, level);
			write_string_len (dstr, ")", 1);
			break;
		case QFBinary:
			write_string_len (dstr, "<", 1);
			binary = (plbinary_t *) item->data;
			write_binary (dstr, binary->data, binary->size);
			write_string_len (dstr, ">", 1);
			break;
		case QFString:
			write_string (dstr, item->data);
			break;
		default:
			break;
	}
}

VISIBLE char *
PL_WritePropertyList (plitem_t *pl)
{
	dstring_t  *dstr = dstring_newstr ();

	if (!quotable_bitmap[0])
		init_quotables ();
	write_item (dstr, pl, 0);
	write_string_len (dstr, "\n", 1);
	return dstring_freeze (dstr);
}

VISIBLE pltype_t
PL_Type (plitem_t *item)
{
	return item->type;
}

VISIBLE int
PL_Line (plitem_t *item)
{
	return item->line;
}
