/*
	qfplist.h

	Property list management types and prototypes

	Copyright (C) 2000 Jeff Teunissen <deek@d2dc.net>

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

#ifndef __QF_qfplist_h_
#define __QF_qfplist_h_

#include "QF/qtypes.h"

//	Ugly defines for fast checking and conversion from char to number
#define inrange(ch,min,max) ((ch) >= (min) && (ch) <= (max))
#define char2num(ch) \
inrange((ch), '0', '9') ? ((ch) - 0x30) \
: (inrange((ch), 'a', 'f') ? ((ch) - 0x57) : ((ch) - 0x37))

// Maximum number of items in an array
#define MAX_ARRAY_INDEX 128

typedef enum {QFDictionary, QFArray, QFBinary, QFString} pltype_t;	// possible types

/*
	Generic property list item
*/
struct plitem_s {
	pltype_t	type;		// Type
	void		*data;
};
typedef struct plitem_s plitem_t;

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
	int				numvals;		// Number of items in array
	struct plitem_s *values[MAX_ARRAY_INDEX+1]; 	// Array data
};
typedef struct plarray_s	plarray_t;

/*
	Typeless, unformatted binary data (not supported yet)
*/
struct plbinary_s {
	size_t		size;
	void		*data;
};
typedef struct plbinary_s	plbinary_t;

struct hashtab_s;

plitem_t *PL_GetPropertyList (const char *);
plitem_t *PL_ObjectForKey (plitem_t *, const char *);
void PL_FreeItem (struct plitem_s *);

typedef struct pldata_s {	// Unparsed property list string
	const char		*ptr;
	unsigned int	end;
	unsigned int	pos;
	unsigned int	line;
	const char		*error;
} pldata_t;

/*
	Internal prototypes

	static plist_t *PL_ParsePropertyList (pldata_t *);
	static qboolean PL_SkipSpace (pldata_t *);
	static char *PL_ParseQuotedString (pldata_t *);
	static char *PL_ParseUnquotedString (pldata_t *);
*/
#endif	// __QF_qfplist_h_
