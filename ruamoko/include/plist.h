#ifndef __ruamoko_plist_h
#define __ruamoko_plist_h

#include <qfile.h>

typedef struct plitem_s *plitem_t;
typedef enum {QFDictionary, QFArray, QFBinary, QFString} pltype_t;	// possible types

@extern plitem_t PL_GetFromFile (QFile file);
@extern plitem_t PL_GetPropertyList (string str);
@extern string PL_WritePropertyList (plitem_t pl);
@extern pltype_t PL_Type (plitem_t str);
@extern string PL_String (plitem_t str);
@extern plitem_t PL_ObjectForKey (plitem_t item, string key);
@extern plitem_t PL_RemoveObjectForKey (plitem_t item, string key);
@extern plitem_t PL_ObjectAtIndex (plitem_t item, int index);
@extern plitem_t PL_D_AllKeys (plitem_t item);
@extern int PL_D_NumKeys (plitem_t item);
@extern int PL_D_AddObject (plitem_t dict, string key, plitem_t value);
@extern int PL_A_AddObject (plitem_t array_item, plitem_t item);
@extern int PL_A_NumObjects (plitem_t item);
@extern int PL_A_InsertObjectAtIndex (plitem_t array_item, plitem_t item, int index);
@extern plitem_t PL_RemoveObjectAtIndex (plitem_t array_item, int index);
@extern plitem_t PL_NewDictionary ();
@extern plitem_t PL_NewArray ();
@extern plitem_t PL_NewData (void *data, int len);
@extern plitem_t PL_NewString (string str);
@extern void PL_Free (plitem_t pl);

#endif//__ruamoko_plist_h
