/*
	qfplist.h

	Property list management types and prototypes

	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>

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

	$Id$
*/

#ifndef __qfplist_h_
#define __qfplist_h_

#include "qtypes.h"

//	Ugly defines for fast checking and conversion from char to number
#define inrange(ch,min,max) ((ch) >= (min) && (ch) <= (max))
#define char2num(ch) \
inrange(ch, '0', '9') ? (ch - 0x30) \
: (inrange(ch, 'a', 'f') ? (ch - 0x57) : (ch - 0x37))

// Maximum number of items in an array
#define MAX_ARRAY_INDEX 128

typedef enum {QFDictionary, QFArray, QFBinary, QFString} pltype_t;	// possible types

struct plitem_s {
	struct plitem_s	*next;	// Pointer to next item
	pltype_t		type;	// Type
	union shared {			// Type-dependant data
		struct dict_s	*dict;
		struct array_s 	*array;
		void			*binary;
		char			*string;
	} data;
};

/*
	Dictionaries
*/
struct dict_s {
	int					numkeys;	// Number of items in dictionary
	struct dictkey_s	*keys;
};

struct dictkey_s {
	struct dictkey_s	*next;
	struct plitem_s		*key;
	struct plitem_s		*value;
};

/*
	Arrays
*/
struct array_s {
	int				numvals;					// Number of items in array
	struct plitem_s *values[MAX_ARRAY_INDEX+1]; 	// Array data
};

// now that we've defined the structs, define their types so we can use them
typedef struct plitem_s 	plitem_t;
typedef struct dict_s		dict_t;
typedef struct dictkey_s	dictkey_t;
typedef struct array_s		array_t;

typedef struct pldata_s {	// Unparsed property list string
	const char		*ptr;
	unsigned int	end;
	unsigned int	pos;
	unsigned int	line;
	char			*error;
} pldata_t;

static plitem_t *PL_GetPropertyList (const char *);

/*
	Internal prototypes

	static plist_t *PL_ParsePropertyList (pldata_t *);
	static qboolean PL_SkipSpace (pldata_t *);
	static char *PL_ParseQuotedString (pldata_t *);
	static char *PL_ParseUnquotedString (pldata_t *);
*/
#endif	// __qfplist_h_
