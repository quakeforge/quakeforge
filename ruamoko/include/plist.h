#ifndef __ruamoko_plist_h
#define __ruamoko_plist_h

struct plitem_s {integer dummy;};
#define PL_TEST(item) (item.dummy)
typedef struct plitem_s plitem_t;
typedef enum {QFDictionary, QFArray, QFBinary, QFString} pltype_t;	// possible types

@extern plitem_t (string str) PL_GetPropertyList;
@extern string (plitem_t pl) PL_WritePropertyList;
@extern pltype_t (plitem_t str) PL_Type;
@extern string (plitem_t str) PL_String;
@extern plitem_t (plitem_t item, string key) PL_ObjectForKey;
@extern plitem_t (plitem_t item, string key) PL_RemoveObjectForKey;
@extern plitem_t (plitem_t item, integer index) PL_ObjectAtIndex;
@extern plitem_t (plitem_t item) PL_D_AllKeys;
@extern integer (plitem_t item) PL_D_NumKeys;
@extern integer (plitem_t dict, string key, plitem_t value) PL_D_AddObject;
@extern integer (plitem_t array_item, plitem_t item) PL_A_AddObject;
@extern integer (plitem_t item) PL_A_NumObjects;
@extern integer (plitem_t array_item, plitem_t item, integer index) PL_A_InsertObjectAtIndex;
@extern plitem_t (plitem_t array_item, integer index) PL_RemoveObjectAtIndex;
@extern plitem_t () PL_NewDictionary;
@extern plitem_t () PL_NewArray;
@extern plitem_t (void []data, integer len) PL_NewData;
@extern plitem_t (string str) PL_NewString;
@extern void (plitem_t pl) PL_Free;

#endif//__ruamoko_plist_h
