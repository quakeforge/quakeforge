#include "plist.h"

plitem_t (string str) PL_GetPropertyList = #0;
string (plitem_t pl) PL_WritePropertyList = #0;
pltype_t (plitem_t str) PL_Type = #0;
string (plitem_t str) PL_String = #0;
plitem_t (plitem_t item, string key) PL_ObjectForKey = #0;
plitem_t (plitem_t item, integer index) PL_ObjectAtIndex = #0;
plitem_t (plitem_t item) PL_D_AllKeys = #0;
integer (plitem_t item) PL_D_NumKeys = #0;
integer (plitem_t dict, plitem_t key, plitem_t value) PL_D_AddObject = #0;
integer (plitem_t array_item, plitem_t item) PL_A_AddObject = #0;
integer (plitem_t item) PL_A_NumObjects = #0;
integer (plitem_t array_item, plitem_t item, integer index) PL_A_InsertObjectAtIndex = #0;
plitem_t () PL_NewDictionary = #0;
plitem_t () PL_NewArray = #0;
plitem_t (void []data, integer len) PL_NewData = #0;
plitem_t (string str) PL_NewString = #0;
void (plitem_t pl) PL_Free = #0;
