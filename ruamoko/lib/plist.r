#include "plist.h"

plitem_t (string str) PL_GetPropertyList = #0;
plitem_t (plitem_t item, string key) PL_ObjectForKey = #0;
plitem_t (plitem_t item, integer index) PL_ObjectAtIndex = #0;

integer (plitem_t dict, plitem_t key, plitem_t value) PL_D_AddObject = #0;
integer (plitem_t array_item, plitem_t item) PL_A_AddObject = #0;
integer (plitem_t array_item, plitem_t item, integer index) PL_A_InsertObjectAtIndex = #0;

plitem_t () PL_NewDictionary = #0;
plitem_t () PL_NewArray = #0;
//plitem_t () PL_NewData = #0;
plitem_t (string str) PL_NewString = #0;
