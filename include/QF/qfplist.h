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

/**
	There are four types of data that can be stored in a property list:

		QFDictionary	A list of values, each associated with a key (a C string).
		QFArray			A list of indexed values
		QFString		A string.
		QFBinary		Random binary data. The parser doesn't load these yet.

	In textual form, a dictionary looks like:

	{
		key = value;
	}

	An array looks like:

	(
		value1,
		value2
	)

	An unquoted string may contain only alphanumeric characters and/or the
	underscore character, '_'. Quoted strings may contain whitespace, C escape
	sequences, and so on. The quote character is '"'.

	<!-- in the following paragram, the \< and \> are just < and >. the \ is
		 for doxygen -->
	QFBinary data (though not loaded currently) is hex-encoded and contained
	within angle brackets, \< \>. The length of the encoded data must be an even
	number, so while \<FF00\> is valid, \<F00\> isn't.

	Property lists may contain C-style or BCPL-style comments.
*/
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
	int				maxvals;
	struct plitem_s **values;	 	// Array data
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

/**
	\fn plitem_t *PL_GetPropertyList (const char *string)
	\brief Create an in-memory representation of the contents of a property list
	
	\param string	the saved plist, as read from a file.

	\return Returns an object equivalent to the passed-in string.
	You are responsible for freeing the object returned.
*/
plitem_t *PL_GetPropertyList (const char *);

/**
	\fn plitem_t *PL_ObjectForKey (plitem_t *dict, const char *key)
	\brief Retrieve a value from a dictionary object.

	\param dict	The dictionary to retrieve a value from
	\param key	The unique key associated with the value

	\return	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
plitem_t *PL_ObjectForKey (plitem_t *, const char *);

/**
	\fn plitem_t *PL_ObjectAtIndex (plitem_t *array, int index)
	\brief Retrieve a value from an array object.

	\param array	The array to get the value from
	\param index	The index within the array to retrieve

	\return	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
plitem_t *PL_ObjectAtIndex (plitem_t *, int);

/**
	\fn plitem_t *PL_D_AllKeys (plitem_t *dict)
	\brief Retrieve a list of all keys in a dictionary.

	\param dict The dictionary to list

	\return Returns an Array containing Strings. You are responsible for
	freeing this array.
*/
plitem_t *PL_D_AllKeys (plitem_t *);

/**
	\fn qboolean PL_D_AddObject (plitem_t *dict, plitem_t *key, plitem_t *value)

	\param dict The dictionary to add the key/value pair to
	\param key The key of the key/value pair to be added to the dictionary
	\param value The value of the key/value pair to be added to the dictionary

	\return true on success, false on failure

	Note: the dictionary becomes the owner of both the key and
	the value.
*/
qboolean PL_D_AddObject (plitem_t *, plitem_t *, plitem_t *);

/**
	\fn qboolean PL_A_AddObject (plitem_t *array, plitem_t *item)
	\param array The array to add the item to
	\param item The item to be added to the array

	\return true on success, false on failure

	Note: the array becomes the owner of the added item.
*/
qboolean PL_A_AddObject (plitem_t *, plitem_t *);

/**
	\fn qboolean PL_A_InsertObjectAtIndex (plitem_t *array, plitem_t *item, int index)
	\param array The array to add the item to
	\param item The item to be added to the array
	\param index The location at which to insert the item into the array

	\return true on success, false on failure

	Note: the array becomes the owner of the added item.
*/
qboolean PL_A_InsertObjectAtIndex (plitem_t *, plitem_t *, int);

plitem_t *PL_NewDictionary (void);
plitem_t *PL_NewArray (void);
plitem_t *PL_NewData (void *, int);
plitem_t *PL_NewString (const char *);

/**
	\fn void PL_Free (plitem_t *object)
	\brief Free a property list object

	This function takes care of freeing any referenced property list data, so
	only call it on top-level objects.
*/
void PL_Free (plitem_t *);

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
