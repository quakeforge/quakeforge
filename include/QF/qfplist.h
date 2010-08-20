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

/** \defgroup qfplist Property lists
	\ingroup utils
*/
//@{

#include "QF/qtypes.h"

/**
	There are four types of data that can be stored in a property list:

	<dl>
		<dt>QFDictionary</dt>	<dd>A list of values, each associated with a
								key (a C string).</dd>
		<dt>QFArray</dt>		<dd>A list of indexed values</dd>
		<dt>QFString</dt>		<dd>A string.</dd>
		<dt>QFBinary</dt>		<dd>Random binary data.</dd>
	</dl>

	In textual form, a dictionary looks like:
	\code
	{
		key = value;
	}
	\endcode
	An array looks like:
	\code
	(
		value1,
		value2
	)
	\endcode
	An unquoted string may contain only alphanumeric characters and/or the
	underscore character, <code>_</code>. Quoted strings may contain
	whitespace, C escape sequences, and so on. The quote character is
	<code>\"</code>. Optionally, Python style long strings
	(<code>\"\"\"...\"\"\"</code>) may be used, allowing for unquoted
	<code>"</code> quotes in the string.

	<!-- in the following paragraph, the \< and \> are just < and >. the \ is
		 for doxygen -->
	QFBinary data is hex-encoded and contained within angle brackets, \c \<
	\c \>.  The length of the encoded data must be an even number, so while 
	\c \<FF00\> is valid, \c \<F00\> isn't.

	Property lists may contain C-style or BCPL-style comments.
*/
typedef enum {QFDictionary, QFArray, QFBinary, QFString} pltype_t;

/**	Generic property list item.

	All inspection and manipulation is to be done via the accessor functions.
*/
typedef struct plitem_s plitem_t;

/** Create an in-memory representation of the contents of a property list.
	
	\param string	the saved plist, as read from a file.

	\return Returns an object equivalent to the passed-in string.
	\note You are responsible for freeing the object returned.
*/
plitem_t *PL_GetPropertyList (const char *string);

/** Create a property list string from the in-memory representation.

	\param pl the in-memory representation
	\return the text representation of the property list
	\note You are responsible for freeing the string returned.
*/
char *PL_WritePropertyList (plitem_t *pl);

/** Retrieve the type of an object.

	\param item The object
	\return the type of the object
*/
pltype_t PL_Type (plitem_t *item);

/** Retrieve a string from a string object.

	\param string The string object
	\return pointer to the actual string value or NULL if string isn't a
	string.
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
const char *PL_String (plitem_t *string);

/** Retrieve a value from a dictionary object.

	\param dict	The dictionary to retrieve a value from
	\param key	The unique key associated with the value
	\return the value associated with the key, or NULL if not found or dict
	isn't a dictionary.
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
plitem_t *PL_ObjectForKey (plitem_t *dict, const char *key);

/** Remove a value from a dictionary object.

	\param dict	The Dictionary to remove the value from
	\param key	The unique key associated with the value to be removed
	\return the value associated with the key, or NULL if not found or dict
	isn't a dictionary.
	\note	You are responsible for freeing the returned object.
*/
plitem_t *PL_RemoveObjectForKey (plitem_t *dict, const char *key);

/** Retrieve a value from an array object.

	\param array	The array to get the value from
	\param index	The index within the array to retrieve
	\return the value associated with the key, or NULL if not found or array
	isn't an array.
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
plitem_t *PL_ObjectAtIndex (plitem_t *array, int index);

/** Retrieve a list of all keys in a dictionary.

	\param dict The dictionary to list
	\return an Array containing Strings or NULL if dict isn't a dictionary
	\note You are responsible for freeing this array.
*/
plitem_t *PL_D_AllKeys (plitem_t *dict);

/** Retrieve the number of keys in a dictionary.

	\param dict The dictionary to get the number of keys of.

	\return Returns the number of keys in the dictionary.
*/
int PL_D_NumKeys (plitem_t *dict);

/** Add a key/value pair to a dictionary.

	\param dict The dictionary to add the key/value pair to
	\param key The key of the key/value pair to be added to the dictionary
	\param value The value of the key/value pair to be added to the dictionary

	\return true on success, false on failure

	\note the dictionary becomes the owner of the value.
*/
qboolean PL_D_AddObject (plitem_t *dict, const char *key, plitem_t *value);

/** Add an item to an array.

	\param array The array to add the item to
	\param item The item to be added to the array

	\return true on success, false on failure

	\note the array becomes the owner of the added item.
*/
qboolean PL_A_AddObject (plitem_t *array, plitem_t *item);

/** Retrieve the number of items in an array.

	\param array The array to get the number of objects in

	\return number of objects in the array
*/
int PL_A_NumObjects (plitem_t *array);

/** Insert an item into an array before the specified location.

	\param array The array to add the item to
	\param item The item to be added to the array
	\param index The location at which to insert the item into the array

	\return true on success, false on failure

	\note the array becomes the owner of the added item.
*/
qboolean PL_A_InsertObjectAtIndex (plitem_t *array, plitem_t *item, int index);

/** Remove a value from an array object.
	The array items will be shuffled to fill the resulting hole.

	\param array	The array to get the value from
	\param index	The index within the array to remove
	\return the value associated with the index, or NULL if not found or array
	isn't an array.
	\note	You are responsible for freeing the returned object.
*/
plitem_t *PL_RemoveObjectAtIndex (plitem_t *array, int index);

/** Create a new dictionary object.
	The dictionary will be empty.
	\return the new dictionary object
*/
plitem_t *PL_NewDictionary (void);

/** Create a new array object.
	The array will be empty.
	\return the new array object
*/
plitem_t *PL_NewArray (void);

/** Create a new data object from the given data.
	Takes ownership of the given data.
	\param data	pointer to data buffer
	\param size	number of bytes in the buffer
	\return the new dictionary object
	\note The data will be freed via free() when the item is freed.
*/
plitem_t *PL_NewData (void *data, size_t size);

/** Create a new string object.
	Makes a copy of the given string.
	\param str	C string to copy
	\return the new dictionary object
*/
plitem_t *PL_NewString (const char *str);

/** Free a property list object.

	This function takes care of freeing any referenced property list data, so
	call it only on top-level objects.

	\param item the property list object to be freed
*/
void PL_Free (plitem_t *item);

//@}

#endif	// __QF_qfplist_h_
