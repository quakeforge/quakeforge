#ifndef __ruamoko_plist_h
#define __ruamoko_plist_h

struct plitem_t = {integer [2] dummy;};

plitem_t (string str) PL_GetPropertyList;
plitem_t (plitem_t item, string key) PL_ObjectForKey;
plitem_t (plitem_t item, integer index) PL_ObjectAtIndex;

plitem_t (plitem_t dict, plitem_t key, plitem_t value) PL_D_AddObject;
plitem_t (plitem_t array_item, plitem_t item) PL_A_AddObject;
plitem_t (plitem_t array_item, plitem_t item, integer index) PL_A_InsertObjectAtIndex;

plitem_t () PL_NewDictionary;
plitem_t () PL_NewArray;
//plitem_t () PL_NewData;
plitem_t (string str) PL_NewString;

#endif//__ruamoko_plist_h
