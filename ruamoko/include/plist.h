#ifndef __ruamoko_plist_h
#define __ruamoko_plist_h

struct plitem_s = {integer [2] dummy;};
typedef struct plitem_s plitem_t;

@extern plitem_t (string str) PL_GetPropertyList;
@extern plitem_t (plitem_t item, string key) PL_ObjectForKey;
@extern plitem_t (plitem_t item, integer index) PL_ObjectAtIndex;

@extern plitem_t (plitem_t dict, plitem_t key, plitem_t value) PL_D_AddObject;
@extern plitem_t (plitem_t array_item, plitem_t item) PL_A_AddObject;
@extern plitem_t (plitem_t array_item, plitem_t item, integer index) PL_A_InsertObjectAtIndex;

@extern plitem_t () PL_NewDictionary;
@extern plitem_t () PL_NewArray;
//@extern plitem_t () PL_NewData;
@extern plitem_t (string str) PL_NewString;

#endif//__ruamoko_plist_h
