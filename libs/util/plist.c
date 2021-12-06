/*
	plist.c

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

#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/plist.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"

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

struct pldict_s {
	hashtab_t  *tab;
	struct DARRAY_TYPE (dictkey_t *) keys;
};
typedef struct pldict_s     pldict_t;

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
	const char *ptr;
	unsigned    end;
	unsigned    pos;
	unsigned    line;
	plitem_t   *error;
	va_ctx_t   *va_ctx;
	hashlink_t **hashlinks;
} pldata_t;

//	Ugly defines for fast checking and conversion from char to number
#define inrange(ch,min,max) ((ch) >= (min) && (ch) <= (max))
#define char2num(ch) \
	(inrange((ch), '0', '9') ? ((ch) - '0') \
							 : 10 + (inrange((ch), 'a', 'f') ? ((ch) - 'a') \
															 : ((ch) - 'A')))

static const char *pl_types[] = {
	"dictionary",
	"array",
	"biinary",
	"string",
};

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

static plitem_t *pl_parsepropertylistitem (pldata_t *);
static qboolean pl_skipspace (pldata_t *);
static char *pl_parsequotedstring (pldata_t *);
static char *pl_parseunquotedstring (pldata_t *);
static char *pl_parsedata (pldata_t *, int *);

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
pl_newitem (pltype_t type)
{
	plitem_t   *item = calloc (1, sizeof (plitem_t));
	item->type = type;
	return item;
}

VISIBLE plitem_t *
PL_NewDictionary (hashlink_t **hashlinks)
{
	plitem_t   *item = pl_newitem (QFDictionary);
	pldict_t   *dict = malloc (sizeof (pldict_t));
	dict->tab = Hash_NewTable (1021, dict_get_key, dict_free, NULL, hashlinks);
	DARRAY_INIT (&dict->keys, 8);
	item->data = dict;
	return item;
}

VISIBLE plitem_t *
PL_NewArray (void)
{
	plitem_t   *item = pl_newitem (QFArray);
	plarray_t  *array = calloc (1, sizeof (plarray_t));
	item->data = array;
	return item;
}

VISIBLE plitem_t *
PL_NewData (void *data, size_t size)
{
	plitem_t   *item = pl_newitem (QFBinary);
	plbinary_t *bin = malloc (sizeof (plbinary_t));
	item->data = bin;
	bin->data = data;
	bin->size = size;
	return item;
}

static plitem_t *
new_string (char *str, int line)
{
	plitem_t   *item = pl_newitem (QFString);
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
	pldict_t   *dict;
	plarray_t  *array;

	if (!item) {
		return;
	}
	switch (item->type) {
		case QFDictionary:
			dict = item->data;
			Hash_DelTable (dict->tab);
			DARRAY_CLEAR (&dict->keys);
			free (item->data);
			break;

		case QFArray:
			{
				array = item->data;
				int 	i = array->numvals;

				while (i-- > 0) {
					PL_Free (array->values[i]);
				}
				free (array->values);
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
		case QFMultiType:
			break;
	}
	free (item);
}

VISIBLE size_t
PL_BinarySize (const plitem_t *binary)
{
	if (!binary || binary->type != QFBinary) {
		return 0;
	}

	plbinary_t *bin = (plbinary_t *) binary->data;
	return bin->size;
}

VISIBLE const void *
PL_BinaryData (const plitem_t *binary)
{
	if (!binary || binary->type != QFBinary) {
		return 0;
	}

	plbinary_t *bin = (plbinary_t *) binary->data;
	return bin->data;
}

VISIBLE const char *
PL_String (const plitem_t *string)
{
	if (!string || string->type != QFString) {
		return NULL;
	}
	return string->data;
}

VISIBLE plitem_t *
PL_ObjectForKey (const plitem_t *item, const char *key)
{
	if (!item || item->type != QFDictionary) {
		return NULL;
	}

	pldict_t   *dict = (pldict_t *) item->data;
	dictkey_t  *k = (dictkey_t *) Hash_Find (dict->tab, key);
	return k ? k->value : NULL;
}

VISIBLE const char *
PL_KeyAtIndex (const plitem_t *item, int index)
{
	if (!item || item->type != QFDictionary) {
		return NULL;
	}

	pldict_t   *dict = (pldict_t *) item->data;
	if (index < 0 || (size_t) index >= dict->keys.size) {
		return NULL;
	}

	return dict->keys.a[index]->key;
}

VISIBLE plitem_t *
PL_RemoveObjectForKey (plitem_t *item, const char *key)
{
	if (!item || item->type != QFDictionary) {
		return NULL;
	}

	pldict_t   *dict = (pldict_t *) item->data;
	dictkey_t  *k;
	plitem_t   *value;

	k = (dictkey_t *) Hash_Del (dict->tab, key);
	if (!k)
		return NULL;
	value = k->value;
	k->value = 0;
	for (size_t i = 0; i < dict->keys.size; i++) {
		if (dict->keys.a[i] == k) {
			DARRAY_REMOVE_AT (&dict->keys, i);
			break;
		}
	}
	dict_free (k, 0);
	return value;
}

VISIBLE plitem_t *
PL_D_AllKeys (const plitem_t *item)
{
	if (!item || item->type != QFDictionary) {
		return NULL;
	}

	pldict_t   *dict = (pldict_t *) item->data;
	dictkey_t  *current;
	plitem_t   *array;

	if (!(array = PL_NewArray ()))
		return NULL;

	for (size_t i = 0; i < dict->keys.size; i++) {
		current = dict->keys.a[i];
		PL_A_AddObject (array, PL_NewString (current->key));
	}

	return array;
}

VISIBLE int
PL_D_NumKeys (const plitem_t *item)
{
	if (!item || item->type != QFDictionary) {
		return 0;
	}

	pldict_t   *dict = (pldict_t *) item->data;
	return Hash_NumElements (dict->tab);
}

VISIBLE plitem_t *
PL_ObjectAtIndex (const plitem_t *array, int index)
{
	if (!array || array->type != QFArray) {
		return NULL;
	}

	plarray_t  *arr = (plarray_t *) array->data;
	return index >= 0 && index < arr->numvals ? arr->values[index] : NULL;
}

VISIBLE qboolean
PL_D_AddObject (plitem_t *item, const char *key, plitem_t *value)
{
	if (!item || item->type != QFDictionary || !value) {
		return false;
	}

	pldict_t   *dict = (pldict_t *) item->data;
	dictkey_t	*k;

	if ((k = Hash_Find (dict->tab, key))) {
		PL_Free ((plitem_t *) k->value);
		k->value = value;
	} else {
		k = malloc (sizeof (dictkey_t));

		if (!k)
			return false;

		k->key = strdup (key);
		k->value = value;

		Hash_Add (dict->tab, k);
		DARRAY_APPEND (&dict->keys, k);
	}
	return true;
}

VISIBLE qboolean
PL_A_InsertObjectAtIndex (plitem_t *array, plitem_t *item, int index)
{
	if (!array || array->type != QFArray || !item) {
		return false;
	}

	plarray_t  *arr;

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
PL_A_NumObjects (const plitem_t *array)
{
	if (!array || array->type != QFArray) {
		return 0;
	}
	return ((plarray_t *) array->data)->numvals;
}

VISIBLE plitem_t *
PL_RemoveObjectAtIndex (plitem_t *array, int index)
{
	if (!array || array->type != QFArray) {
		return 0;
	}

	plarray_t  *arr;
	plitem_t   *item;

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
pl_skipspace (pldata_t *pl)
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
						pl->error = PL_NewString ("Reached end of string in comment");
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
						pl->error = PL_NewString ("Reached end of string in comment");
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
	pl->error = PL_NewString ("Reached end of string");
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
pl_parsedata (pldata_t *pl, int *len)
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
				pl->error = PL_NewString ("invalid data, missing nibble");
				return NULL;
			}
			*len = nibbles / 2;
			str = malloc (*len);
			for (i = 0; i < *len; i++)
				str[i] = make_byte (pl->ptr[start + i * 2],
									pl->ptr[start + i * 2 + 1]);
			return str;
		}
		pl->error = PL_NewString (va (pl->va_ctx,
									  "invalid character in data: %02x", c));
		return NULL;
	}
	pl->error = PL_NewString ("Reached end of string while parsing data");
	return NULL;
}

static char *
pl_parsequotedstring (pldata_t *pl)
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
		pl->error = PL_NewString ("Reached end of string while parsing quoted string");
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
pl_parseunquotedstring (pldata_t *pl)
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
pl_parsepropertylistitem (pldata_t *pl)
{
	plitem_t	*item = NULL;

	if (!pl_skipspace (pl))
		return NULL;

	switch (pl->ptr[pl->pos]) {
	case '{':
	{
		item = PL_NewDictionary (pl->hashlinks);
		item->line = pl->line;

		pl->pos++;

		while (pl_skipspace (pl) && pl->ptr[pl->pos] != '}') {
			plitem_t	*key;
			plitem_t	*value;

			if (!(key = pl_parsepropertylistitem (pl))) {
				PL_Free (item);
				return NULL;
			}

			if (!(pl_skipspace (pl))) {
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}

			if (key->type != QFString) {
				pl->error = PL_NewString ("Key is not a string");
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}

			if (pl->ptr[pl->pos] != '=') {
				pl->error = PL_NewString (va (pl->va_ctx, "Unexpected character %c (expected '=')", pl->ptr[pl->pos]));
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}
			pl->pos++;

			// If there is no value, lose the key
			if (!(value = pl_parsepropertylistitem (pl))) {
				PL_Free (key);
				PL_Free (item);
				return NULL;
			}

			if (!(pl_skipspace (pl))) {
				PL_Free (key);
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			if (pl->ptr[pl->pos] == ';') {
				pl->pos++;
			} else if (pl->ptr[pl->pos] != '}') {
				pl->error = PL_NewString (va (pl->va_ctx, "Unexpected character %c (wanted ';' or '}')", pl->ptr[pl->pos]));
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
			pl->error = PL_NewString ("Unexpected end of string when parsing dictionary");
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

		while (pl_skipspace (pl) && pl->ptr[pl->pos] != ')') {
			plitem_t	*value;

			if (!(value = pl_parsepropertylistitem (pl))) {
				PL_Free (item);
				return NULL;
			}

			if (!(pl_skipspace (pl))) {
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			if (pl->ptr[pl->pos] == ',') {
				pl->pos++;
			} else if (pl->ptr[pl->pos] != ')') {
				pl->error = PL_NewString (va (pl->va_ctx, "Unexpected character %c (wanted ',' or ')')", pl->ptr[pl->pos]));
				PL_Free (value);
				PL_Free (item);
				return NULL;
			}

			if (!PL_A_AddObject (item, value)) {
				pl->error = PL_NewString ("Unexpected character (too many items in array)");
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
		char *str = pl_parsedata (pl, &len);

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
		char *str = pl_parsequotedstring (pl);

		if (!str) {
			return NULL;
		} else {
			return new_string (str, line);
		}
	}

	default: {
		int line = pl->line;
		char *str = pl_parseunquotedstring (pl);

		if (!str) {
			return NULL;
		} else {
			return new_string (str, line);
		}
	}
	} // switch
}

VISIBLE plitem_t *
PL_GetPropertyList (const char *string, hashlink_t **hashlinks)
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
	pl->va_ctx = va_create_context (4);
	pl->hashlinks = hashlinks;

	if ((newpl = pl_parsepropertylistitem (pl))) {
		va_destroy_context (pl->va_ctx);
		free (pl);
		return newpl;
	} else {
		if (pl && pl->error) {
			const char *error = PL_String (pl->error);
			if (error[0]) {
				Sys_Printf ("plist: %d,%d: %s\n", pl->line, pl->pos, error);
			}
			PL_Free (pl->error);
		}
		va_destroy_context (pl->va_ctx);
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
write_item (dstring_t *dstr, const plitem_t *item, int level)
{
	dictkey_t	*current;
	plarray_t	*array;
	pldict_t    *dict;
	plbinary_t	*binary;
	int			i;

	switch (item->type) {
		case QFDictionary:
			write_string_len (dstr, "{\n", 2);
			dict = (pldict_t *) item->data;
			for (size_t i = 0; i < dict->keys.size; i++) {
				current = dict->keys.a[i];
				write_tabs (dstr, level + 1);
				write_string (dstr, current->key);
				write_string_len (dstr, " = ", 3);
				write_item (dstr, current->value, level + 1);
				write_string_len (dstr, ";\n", 2);
			}
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
PL_WritePropertyList (const plitem_t *pl)
{
	dstring_t  *dstr = dstring_newstr ();

	if (pl) {
		if (!quotable_bitmap[0])
			init_quotables ();
		write_item (dstr, pl, 0);
		write_string_len (dstr, "\n", 1);
	}
	return dstring_freeze (dstr);
}

VISIBLE pltype_t
PL_Type (const plitem_t *item)
{
	return item->type;
}

VISIBLE int
PL_Line (const plitem_t *item)
{
	return item->line;
}

VISIBLE void
PL_Message (plitem_t *messages, const plitem_t *item, const char *fmt, ...)
{
	va_list     args;
	dstring_t  *va_str;
	dstring_t  *msg_str;
	char       *msg;

	va_str = dstring_new ();
	msg_str = dstring_new ();

	va_start (args, fmt);
	dvsprintf (va_str, fmt, args);
	va_end (args);

	if (item) {
		msg = dsprintf (msg_str, "%d: %s", item->line, va_str->str);
	} else {
		msg = dsprintf (msg_str, "internal: %s", va_str->str);
	}
	PL_A_AddObject (messages, PL_NewString (msg));
	dstring_delete (va_str);
	dstring_delete (msg_str);
}

static int
pl_default_parser (const plfield_t *field, const plitem_t *item, void *data,
				   plitem_t *messages, void *context)
{
	switch (field->type) {
		case QFDictionary:
			{
				*(hashtab_t **)data = ((pldict_t *)item->data)->tab;
			}
			return 1;
		case QFArray:
			{
				plarray_t  *array = (plarray_t *)item->data;
				typedef struct DARRAY_TYPE (plitem_t *) arraydata_t;
				arraydata_t *arraydata = DARRAY_ALLOCFIXED(arraydata_t,
														   array->numvals,
														   malloc);
				memcpy (arraydata->a, array->values,
						array->numvals * sizeof (arraydata->a[0]));
			}
			return 1;
		case QFBinary:
			{
				plbinary_t *binary = (plbinary_t *)item->data;
				typedef struct DARRAY_TYPE (byte) bindata_t;
				bindata_t  *bindata = DARRAY_ALLOCFIXED(bindata_t,
														binary->size, malloc);
				memcpy (bindata->a, binary->data, binary->size);
				*(bindata_t **)data = bindata;
			}
			return 1;
		case QFString:
			*(char **)data = (char *)item->data;
			return 1;
		case QFMultiType:
			break;
	}
	PL_Message (messages, 0, "invalid item type: %d", field->type);
	return 0;
}

VISIBLE int
PL_CheckType (pltype_t field_type, pltype_t item_type)
{
	if (field_type & QFMultiType) {
		// field_type is a mask of allowed types
		return field_type & (1 << item_type);
	} else {
		// exact match
		return field_type == item_type;
	}
}

VISIBLE void
PL_TypeMismatch (plitem_t *messages, const plitem_t *item, const char *name,
				 pltype_t field_type, pltype_t item_type)
{
	const int num_types = sizeof (pl_types) / sizeof (pl_types[0]);
	if (field_type & QFMultiType) {
		PL_Message (messages, item,
					"error: %s is the wrong type. Got %s, expected on of:",
					name, pl_types[item_type]);
		field_type &= ~QFMultiType;
		for (int type = 0; field_type && type < num_types;
			 type++, field_type >>= 1) {
			if (field_type & 1) {
				PL_Message (messages, item, "    %s", pl_types[type]);
			}
		}
	} else {
		PL_Message (messages, item,
					"error: %s is the wrong type. Got %s, expected %s", name,
					pl_types[item_type], pl_types[field_type]);
	}
}

VISIBLE int
PL_ParseStruct (const plfield_t *fields, const plitem_t *item, void *data,
				plitem_t *messages, void *context)
{
	pldict_t   *dict = item->data;
	dictkey_t  *current;
	int         result = 1;
	plparser_t  parser;

	if (item->type != QFDictionary) {
		PL_Message (messages, item, "error: not a dictionary object");
		return 0;
	}


	for (size_t i = 0; i < dict->keys.size; i++) {
		const plfield_t *f;
		current = dict->keys.a[i];
		for (f = fields; f->name; f++) {
			if (strcmp (f->name, current->key) == 0) {
				plitem_t   *item = current->value;
				void       *flddata = (byte *)data + f->offset;

				if (f->parser) {
					parser = f->parser;
				} else {
					parser = pl_default_parser;
				}
				if (!PL_CheckType (f->type, item->type)) {
					PL_TypeMismatch (messages, item, current->key,
									 f->type, item->type);
					result = 0;
				} else {
					if (!parser (f, item, flddata, messages, context)) {
						result = 0;
					}
				}
				break;
			}
		}
		if (!f->name) {
			PL_Message (messages, item, "error: unknown field %s",
						current->key);
			result = 0;
		}
	}
	return result;
}

VISIBLE int
PL_ParseArray (const plfield_t *field, const plitem_t *array, void *data,
			   plitem_t *messages, void *context)
{
	int         result = 1;
	plparser_t  parser;
	plarray_t  *plarray = (plarray_t *) array->data;
	plelement_t *element = (plelement_t *) field->data;
	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;
	plfield_t   f = { 0, 0, element->type, element->parser, element->data };

	if (array->type != QFArray) {
		PL_Message (messages, array, "error: not an array object");
		return 0;
	}
	if (f.parser) {
		parser = f.parser;
	} else {
		parser = pl_default_parser;
	}

	arr = DARRAY_ALLOCFIXED_OBJ (arr_t, plarray->numvals * element->stride,
								 element->alloc, context);
	memset (arr->a, 0, arr->size);
	// the array is allocated using bytes, but need the actual number of
	// elements in the array
	arr->size = arr->maxSize = plarray->numvals;

	for (int i = 0; i < plarray->numvals; i++) {
		plitem_t   *item = plarray->values[i];
		void       *eledata = &arr->a[i * element->stride];

		if (!PL_CheckType (element->type, item->type)) {
			char        index[16];
			snprintf (index, sizeof(index) - 1, "%d", i);
			index[15] = 0;
			PL_TypeMismatch (messages, item, index, element->type, item->type);
			result = 0;
		} else {
			if (!parser (&f, item, eledata, messages, context)) {
				result = 0;
			}
		}
	}
	*(arr_t **) data = arr;
	return result;
}

VISIBLE int
PL_ParseLabeledArray (const plfield_t *field, const plitem_t *item,
					  void *data, plitem_t *messages, void *context)
{
	pldict_t   *dict = item->data;
	dictkey_t  *current;
	int         result = 1;
	plparser_t  parser;
	plelement_t *element = (plelement_t *) field->data;
	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;
	plfield_t   f = { 0, 0, element->type, element->parser, element->data };

	if (item->type != QFDictionary) {
		PL_Message (messages, item, "error: not a dictionary object");
		return 0;
	}
	if (f.parser) {
		parser = f.parser;
	} else {
		parser = pl_default_parser;
	}

	arr = DARRAY_ALLOCFIXED_OBJ (arr_t, dict->keys.size * element->stride,
								 element->alloc, context);
	memset (arr->a, 0, arr->size);
	// the array is allocated using bytes, but need the actual number of
	// elements in the array
	arr->size = arr->maxSize = dict->keys.size;

	for (size_t i = 0; i < dict->keys.size; i++) {
		current = dict->keys.a[i];
		plitem_t   *item = current->value;
		void       *eledata = &arr->a[i * element->stride];

		if (!PL_CheckType (element->type, item->type)) {
			char        index[16];
			snprintf (index, sizeof(index) - 1, "%zd", i);
			index[15] = 0;
			PL_TypeMismatch (messages, item, index, element->type, item->type);
			result = 0;
		} else {
			if (!parser (&f, item, eledata, messages, context)) {
				result = 0;
			}
		}
	}
	*(arr_t **) data = arr;
	return result;
}

VISIBLE int
PL_ParseSymtab (const plfield_t *field, const plitem_t *item, void *data,
				plitem_t *messages, void *context)
{
	pldict_t   *dict = item->data;
	dictkey_t  *current;
	int         result = 1;
	plparser_t  parser;
	__auto_type tab = (hashtab_t *) data;

	plelement_t *element = (plelement_t *) field->data;
	plfield_t   f = { 0, 0, element->type, element->parser, element->data };

	if (item->type != QFDictionary) {
		PL_Message (messages, item, "error: not a dictionary object");
		return 0;
	}

	if (f.parser) {
		parser = f.parser;
	} else {
		PL_Message (messages, item, "no parser set");
		return 0;
	}

	void       *obj = element->alloc (context, element->stride);
	memset (obj, 0, element->stride);
	for (size_t i = 0; i < dict->keys.size; i++) {
		current = dict->keys.a[i];
		const char *key = current->key;
		plitem_t   *item = current->value;

		if (!PL_CheckType (element->type, item->type)) {
			PL_TypeMismatch (messages, item, key, element->type, item->type);
			result = 0;
			continue;
		}
		f.name = key;
		if (Hash_Find (tab, key)) {
			PL_Message (messages, item, "duplicate name");
			result = 0;
		} else {
			if (!parser (&f, item, obj, messages, context)) {
				result = 0;
			} else {
				Hash_Add (tab, obj);
				obj = element->alloc (context, element->stride);
				memset (obj, 0, element->stride);
			}
		}
	}
	Hash_Free (tab, obj);
	return result;
}
