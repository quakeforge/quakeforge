/*
	plist.h

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

*/

#ifndef __QF_plist_h
#define __QF_plist_h

struct hashctx_s;

/** \defgroup plist Property lists
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"

/** The type of the property list item.

	For further details, see \ref property-list.
*/
typedef enum {
	QFDictionary,	///< The property list item represents a dictionary.
					///< JSON object.
	QFArray,		///< The property list item represents an array.
					///< JSON array.
	QFBinary,		///< The property list item represents arbitrary binary
					///< data.
	QFString,		///< The property list item represents a C string.
					///< JSON string.
	QFNumber,		///< JSON number
	QFBool,			///< JSON bool value (true/false)
	QFNull,			///< JSON null value

	QFMultiType = (1 << 31)	///< if bit 31 is set, the type indicates a mask
					///< of allowed types for plfield_t
} pltype_t;

/**	Generic property list item.

	All inspection and manipulation is to be done via the accessor functions.
*/
typedef struct plitem_s plitem_t;

struct plfield_s;
/** Custom parser for the field.

	With this, custom parsing of any property list object type is
	supported. For example, parsing of strings into numeric values,
	converting binary objects to images, and deeper parsing of array and
	dictionary objects.

	If null, then the default parser for the object type is used:
	  * QFString: pointer to the actual string. The string continues to
		be owned by the string object.
	  * QFBinary: pointer to fixed-size DARRAY_TYPE(byte) (so size isn't
		lost)
	  * QFArray: pointer to fixed-size DARRAY_TYPE(plitem_t *) with the
		individual objects. The individual objects continue to be owned
		by the array object.
	  * QFDictionary: pointer to the hashtab_t hash table used for the
		dictionary object. The hash table continues to be owned by the
		dictionary object.

	\param field	Pointer to this field item.
	\param item		The property list item being parsed into the field.
	\param data		Pointer to the field in the structure being parsed.
	\param messages	An array object the parser can use to store any
					error messages. Messages should be strings, but no
					checking is done: it is up to the top-level caller to
					parse out the messages.
	\param context	Additional context data passed to the parser.
	\return			0 for error, 1 for success. See \a PL_ParseStruct,
					\a PL_ParseArray, \a PL_ParseLabeledArray, and
					\a PL_ParseSymtab.
*/
typedef int (*plparser_t) (const struct plfield_s *field,
						   const struct plitem_s *item,
						   void *data,
						   struct plitem_s *messages,
						   void *context);

/** A field to be parsed from a dictionary item.

	\a PL_ParseStruct uses an array (terminated by an element with \a name
	set to null) of these to describe the fields in the structure being
	parsed.

	\a PL_ParseArray, \a PL_ParseLabeledArray, and \a PL_ParseSymtab use only
	a single \a plfield_t object, and then only the \a data field, which must
	point to a \a plelement_t object. This allows all the parse functions to
	be used directly as either a \a plfield_t or \a plelement_t object's
	\a parser.

	\a PL_ParseLabeledArray and \a PL_ParseSymtab pass to the parser a field
	with \a name set to the key of the current item and \a offset set to 0.
	For \a PL_ParseArray, \a name is set to null and \a offset is set to the
	current index. \a PL_ParseLabeledArray also sets \a offset to the current
	index. \a type, \a parser, and \a data are taken from the \a plelement_t
	passed in to them.

	\a PL_ParseStruct passes the currently parsed field without any
	modifications.
*/
typedef struct plfield_s {
	const char *name;		///< matched by dictionary key
	size_t      offset;		///< the offset of the field within the structure
	pltype_t    type;		///< the required type of the dictionary object
	plparser_t  parser;		///< custom parser function
	void       *data;		///< additional data for \a parser
} plfield_t;

typedef struct plelement_s {
	pltype_t    type;		///< the required type of the array elements
	size_t      stride;		///< the size of each element
	void     *(*alloc) (void *ctx, size_t size);///< allocator for array memory
	plparser_t  parser;		///< custom parser function
	void       *data;		///< additional data for \a parser
} plelement_t;

/** Create an in-memory representation of the contents of a property list.

	The string is parsed as per the old NextStep property lists, with a few
	extensions.

	\note Does not create number, bool or null items.

	\param string	the saved plist, as read from a file.
	\param hashctx	Hashlink chain to use when creating dictionaries (see
					Hash_NewTable()). May be null.

	\return Returns an object equivalent to the passed-in string.
	\note You are responsible for freeing the returned object.
*/
plitem_t *PL_GetPropertyList (const char *string, struct hashctx_s **hashctx);

/** Create an in-memory representation of the contents of a JSON object.

	The string is parsed as standard JSON (rfc7159)

	\note does not create binary items.

	\param string	the saved JSON, as read from a file.
	\param hashctx	Hashlink chain to use when creating dictionaries (see
					Hash_NewTable()). May be null.

	\return Returns an object equivalent to the passed-in string.
	\note You are responsible for freeing the returned object.
*/
plitem_t *PL_ParseJSON (const char *string, struct hashctx_s **hashctx);

/** Create a property list from a bare dictionary list.

	The input is treated as a list of dictionary key-value pairs without the
	enclosing { or }.

	\param string	dicitionary list string.
	\param hashctx	Hashlink chain to use when creating dictionaries (see
					Hash_NewTable()). May be null.

	\return Returns a dictionary object.
	\note You are responsible for freeing the returned object.
*/
plitem_t *PL_GetDictionary (const char *string, struct hashctx_s **hashctx);

/** Create a property list from a bare array list.

	The input is treated as a list of array values without the enclosing ( or ).

	\param string	array list string.
	\param hashctx	Hashlink chain to use when creating dictionaries (see
					Hash_NewTable()). May be null.

	\return Returns an array object.
	\note You are responsible for freeing the returned object.
*/
plitem_t *PL_GetArray (const char *string, struct hashctx_s **hashctx);

/** Create a property list string from the in-memory representation.

	\param pl the in-memory representation
	\return the text representation of the property list
	\note You are responsible for freeing the returned string.
*/
char *PL_WritePropertyList (const plitem_t *pl);

/** Create a JSON list string from the in-memory representation.

	\param pl the in-memory representation
	\return the text representation of the property list
	\note You are responsible for freeing the returned string.
*/
char *PL_WriteJSON (const plitem_t *pl);

/** Retrieve the type of an object.

	\param item The object. Must not be null.
	\return the type of the object
*/
pltype_t PL_Type (const plitem_t *item) __attribute__((pure));

/** Retrieve the line number of an object.

	\param item The object. Must not be null.
	\return the line number on which the object began, or 0 if not from a
			string
*/
int PL_Line (const plitem_t *item) __attribute__((pure));

/** Retrieve the data size from a binary object.

	\param binary The binary object
	\return the size in bytes of the binary object 0 if \a binary isn't a
	binary object (includes if \a binary is null).
*/
size_t PL_BinarySize (const plitem_t *binary) __attribute__((pure));

/** Retrieve the data from a binary object.

	\param binary The binary object
	\return pointer to the actual data or NULL if \b binary isn't a binary
	object (includes if \a binary is null).
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
const void *PL_BinaryData (const plitem_t *binary) __attribute__((pure));

/** Retrieve a string from a string object.

	\param string The string object
	\return pointer to the actual string value or NULL if string isn't a
	string (includes if \a string is null).
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
const char *PL_String (const plitem_t *string) __attribute__((pure));

double PL_Number (const plitem_t *number) __attribute__((pure));

bool PL_Bool (const plitem_t *boolean) __attribute__((pure));

/** Retrieve a value from a dictionary object.

	\param dict	The dictionary to retrieve a value from
	\param key	The unique key associated with the value
	\return the value associated with the key, or NULL if not found or \a dict
	isn't a dictionary (includes if \a dict is null).
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
plitem_t *PL_ObjectForKey (const plitem_t *dict, const char *key);

/** Remove a value from a dictionary object.

	\param dict	The Dictionary to remove the value from
	\param key	The unique key associated with the value to be removed
*/
void PL_RemoveObjectForKey (plitem_t *dict, const char *key);

/** Retrieve a key from a dictionary object.

	\param dict		The dictionary to get the key from
	\param index	The index of the key
	\return the key at the specified index, or NULL if index is out of range or
	dict is not a dictionary (includes if \a dict is null).
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
const char *PL_KeyAtIndex (const plitem_t *dict, int index) __attribute__((pure));

/** Retrieve a value from an array object.

	\param array	The array to get the value from
	\param index	The index within the array to retrieve
	\return the value at the specified index, or NULL if \a index is out of
	range or \a array is not an array (includes if \a array is null).
	\note	You are NOT responsible for freeing the returned object. It will
	be destroyed when its container is.
*/
plitem_t *PL_ObjectAtIndex (const plitem_t *array, int index) __attribute__((pure));

/** Retrieve a list of all keys in a dictionary.

	\param dict The dictionary to list
	\return an Array containing Strings or NULL if \a dict isn't a dictionary
	(includes if \a dict is null).
	\note You are responsible for freeing this array.
*/
plitem_t *PL_D_AllKeys (const plitem_t *dict);

/** Retrieve the number of keys in a dictionary.

	\param dict The dictionary to get the number of keys of.

	\return Returns the number of keys in the dictionary or 0 if \a dict isn't
	a dictionary (includes if \a dict is null).
*/
int PL_D_NumKeys (const plitem_t *dict) __attribute__((pure));

/** Add a key/value pair to a dictionary.

	\param dict The dictionary to which the key/value pair will be added
	\param key The key of the key/value pair to be added to the dictionary
	\param value The value of the key/value pair to be added to the dictionary

	\return true on success, false on failure (\a dict is null or not a
	dictionary)

	\note the dictionary becomes the owner of the value.
*/
bool PL_D_AddObject (plitem_t *dict, const char *key, plitem_t *value);

/** Copy contents of one dictionary into another.

	The contents of \a srcDict are added to \a dstDict without affecting
	\a srcDict. Any collisions in \a dstDict result in those values in
	\a dstDict being replaced by the the values from \a srcDict: the new
	key-value pairs override the old.

	\param dstDict  The dictionary to extend
	\param srcDict  The dictionary from which key-value pairs will be copied
	\return true if values were copied, false if nothing was copied (either
	dictionary is null, or not a dictionary, or if \a srcDict was empty)
*/
bool PL_D_Extend (plitem_t *dstDict, plitem_t *srcDict);

/** Add an item to an array.

	\param array The array to which the item will be added
	\param item The item to be added to the array

	\return true on success, false on failure (\a array is null or not an
	array)

	\note the array becomes the owner of the added item.
*/
bool PL_A_AddObject (plitem_t *array, plitem_t *item);

/** Append contents of one array to another.

	The contents of \a srcArray are added to \a dstArray without affecting
	\a srcArray. Those values are appended to the destination array values.

	\param dstArray The array to extend
	\param srcArray The array from which values will be copied
	\return true if values were copied, false if nothing was copied (either
	array is null, or not an array, or if \a srcArray was empty)
*/
bool PL_A_Extend (plitem_t *dstArray, plitem_t *srcArray);

/** Retrieve the number of items in an array.

	\param array The array from which to get the number of objects

	\return number of objects in the array or 0 if \a array is null or not
	an array.
*/
int PL_A_NumObjects (const plitem_t *array) __attribute__((pure));

/** Insert an item into an array before the specified location.

	\param array The array to which the item will be added
	\param item The item to be added to the array
	\param index The location at which to insert the item into the array

	\return true on success, false on failure (\a array is null or not an
	array).

	\note the array becomes the owner of the added item.
*/
bool PL_A_InsertObjectAtIndex (plitem_t *array, plitem_t *item, int index);

/** Remove a value from an array object.
	The array items will be shifted to fill the resulting hole.

	\param array	The array from which to remove the value
	\param index	The index within the array to remove
*/
void PL_RemoveObjectAtIndex (plitem_t *array, int index);

/** Create a new dictionary object.
	The dictionary will be empty.
	\param hashctx	Hashlink chain to use when creating dictionaries (see
					Hash_NewTable()). May be null.
	\return the new dictionary object
*/
plitem_t *PL_NewDictionary (struct hashctx_s **hashctx);

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

/** Retain ownership of a property list object.

	Use prior to removal to ensure the property list object is not freed when
	removed from an array or dictionary. Adding an object to a dictionary or
	array automatically retains the object.

	\param item the property list object to be retained
	\return the item
	\note item may be null, in which case nothing is done but to return null
*/
plitem_t *PL_Retain (plitem_t *item);

/** Release ownership of a property list object.

	If the number of owners is reduced to 0 (or is already 0) then the object
	will be freed. If the object contains other objects, then those objects
	will be released.

	\param item the property list object to be released
	\return the item if it is still valid, otherwise null
	\note item may be null, in which case nothing is done but to return null
*/
plitem_t *PL_Release (plitem_t *item);

void PL_SetUserData (plitem_t *item, void *data);
void *PL_GetUserData (plitem_t *item) __attribute__((pure));

int PL_CheckType (pltype_t field_type, pltype_t item_type) __attribute__((const));
void PL_TypeMismatch (plitem_t *messages, const plitem_t *item,
					  const char *name, pltype_t field_type,
					  pltype_t item_type);

/**	Parse a dictionary object into a structure.

	For each key in the dictionary, the corresponding field item is used to
	determine how to parse the object associated with that key. Duplicate
	field items are ignored: only the first item is used, and no checking is
	done. Fields for which there is no key in the dictionary are also ignored,
	and the destination is left unmodified. However, keys that have no
	corresponding field are treated as errors and a suitable message is added
	to the \a messages object.

	When an error occurs (unknown key, incorrect item type (item type does not
	match the type specified in the field item) or the field item's \a parser
	returns 0), processing continues but the error result is returned.

	Can be used recursively to parse deep hierarchies.

	\param fields	Array of field items describing the structure. Terminated
					by a field item with a null \a name pointer.
	\param dict		The dictionary object to parse
	\param data     Pointer to the structure into which the data will be
					parsed.
	\param messages Array object supplied by the caller used for storing
					messages. The messages may or may not indicate errors (its
					contents are not checked). This function itself will add
					only string objects.
					If there are any errors, suitable messages will be found in
					the \a messages object. However, just because there are no
					errors doesn't mean that \a messages will remain empty as
					a field's \a parser may add other messages. The standard
					message format is "[line number]: [message]". If the line
					number is 0, then the actual line is unknown (due to the
					source item not being parsed from a file or string).
	\param context	Additional context data passed to the parser.
	\return			0 if there are any errors, 1 if there are no errors.
*/
int PL_ParseStruct (const plfield_t *fields, const plitem_t *dict,
					void *data, plitem_t *messages, void *context);

/**	Parse an array object into a dynamic array (see darray.h).

	For each object in the array, the field item is used to determine how to
	parse the object. If the array is empty, the destination will be
	initialized to an empty array.

	When an error occurs (incorrect item type (item type does not match the
	type specified in the element object) or the element object's \a parser
	returns 0), processing continues but the error result is returned.

	Can be used recursively to parse deep hierarchies.

	\param field	Pointer to a single field that has the field data pointer
					set to reference a plelement_t object used to describe
					the contents of the array.
	\param array	The array object to parse
	\param data     Pointer to the pointer to which the dynamic array will
					be written. The dynamic array is allocated using
					DARRAY_ALLOCFIXED_OBJ().
	\param messages Array object supplied by the caller used for storing
					messages. The messages may or may not indicate errors (its
					contents are not checked). This function itself will add
					only string objects.
					If there are any errors, suitable messages will be found in
					the \a messages object. However, just because there are no
					errors doesn't mean that \a messages will remain empty as
					a field's \a parser may add other messages. The standard
					message format is "[line number]: [message]". If the line
					number is 0, then the actual line is unknown (due to the
					source item not being parsed from a file or string).
	\param context	Additional context data passed to the parser and allocator.
	\return			0 if there are any errors, 1 if there are no errors.
*/
int PL_ParseArray (const plfield_t *field, const plitem_t *array,
				   void *data, plitem_t *messages, void *context);

/**	Parse a dictionary object into a dynamic array (see darray.h).

	This is useful when the dictionary object is meant to be a labeled list
	rather than a representation of a structure.

	For each object in the array, the field item is used to determine how to
	parse the object. If the array is empty, the destination will be
	initialized to an empty array.

	When an error occurs (incorrect item type (item type does not match the
	type specified in the element object) or the element object's \a parser
	returns 0), processing continues but the error result is returned.

	Can be used recursively to parse deep hierarchies.

	\param field	Pointer to a single field that has the field data pointer
					set to reference a plelement_t object used to describe
					the contents of the array.
	\param dict		The dict object to parse
	\param data     Pointer to the pointer to which the dynamic array will
					be written. The dynamic array is allocated using
					DARRAY_ALLOCFIXED_OBJ().
	\param messages Array object supplied by the caller used for storing
					messages. The messages may or may not indicate errors (its
					contents are not checked). This function itself will add
					only string objects.
					If there are any errors, suitable messages will be found in
					the \a messages object. However, just because there are no
					errors doesn't mean that \a messages will remain empty as
					a field's \a parser may add other messages. The standard
					message format is "[line number]: [message]". If the line
					number is 0, then the actual line is unknown (due to the
					source item not being parsed from a file or string).
	\param context	Additional context data passed to the parser and allocator.
	\return			0 if there are any errors, 1 if there are no errors.
*/
int PL_ParseLabeledArray (const plfield_t *field, const plitem_t *dict,
						  void *data, plitem_t *messages, void *context);

/**	Parse a dictionary object into a hash table.

	For each key in the dictionary, the element object is used to determine
	how to parse the object associated with that key. Duplicate keys are an
	error: they must be unique. A suitable message is added to the
	\a messages object. If the dictionary object is empty, the destination
	table is left unmodified.

	When an error occurs (duplicate keys, incorrect type, or the element
	object's \a parser returns 0), processing continues but the error
	result is returned.

	Can be used recursively to parse deep hierarchies.

	Hash_Add() is used to add objects to the hash table, and Hash_Find() is
	used to check for duplicates. Hash_Free() is used to free unused
	objects. The means that the hash table is expected to use standard
	objects with embedded keys (the parser is expected to put the key in the
	object) and to have a free function.

	The parser's data paramenter points to a pre-allocated block of memory
	of the sized indicated by the element object's size field, using the
	element object's alloc callback. The name field in the field paramenter
	is set to the object's key and remains owned by the dictionary.

	\param field	Pointer to a single field that has the field data pointer
					set to reference a plelement_t object used to describe
					the contents of the dictionary.
	\param dict		The dictionary object to parse
	\param data     Pointer to the structure into which the data will be
					parsed.
	\param messages Array object supplied by the caller used for storing
					messages. The messages may or may not indicate errors (its
					contents are not checked). This function itself will add
					only string objects.
					If there are any errors, suitable messages will be found in
					the \a messages object. However, just because there are no
					errors doesn't mean that \a messages will remain empty as
					a field's \a parser may add other messages. The standard
					message format is "[line number]: [message]". If the line
					number is 0, then the actual line is unknown (due to the
					source item not being parsed from a file or string).
	\param context	Additional context data passed to the parser and allocator.
	\return			0 if there are any errors, 1 if there are no errors.
*/
int PL_ParseSymtab (const plfield_t *field, const plitem_t *dict,
					void *data, plitem_t *messages, void *context);
void __attribute__((format(PRINTF,3,4)))
PL_Message (plitem_t *messages, const plitem_t *item, const char *fmt, ...);

///@}

#endif//__QF_plist_h
