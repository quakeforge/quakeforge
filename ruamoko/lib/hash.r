#include "hash.h"

hashtab_t (integer size, string (void []ele, void []data) gk, void (void []ele, void []data) f, void [] ud) Hash_NewTable = #0;
void (hashtab_t tab, unsigned (void []ele, void []data) gh, integer (void [] ele1, void [] ele2, void [] data) cmp) Hash_SetHashCompare = #0;
void (hashtab_t tab) Hash_DelTable = #0;
void (hashtab_t tab) Hash_FlushTable = #0;
integer (hashtab_t tab, void [] ele) Hash_Add = #0;
integer (hashtab_t tab, void [] ele) Hash_AddElement = #0;
(void []) (hashtab_t tab, string key) Hash_Find = #0;
(void []) (hashtab_t tab, void [] ele) Hash_FindElement = #0;
(void [][]) (hashtab_t tab, string key) Hash_FindList = #0;
(void [][]) (hashtab_t tab, void [] ele) Hash_FindElementList = #0;
(void []) (hashtab_t tab, string key) Hash_Del = #0;
(void []) (hashtab_t tab, void [] ele) Hash_DelElement = #0;
void (hashtab_t tab, void [] ele) Hash_Free = #0;
integer (string str) Hash_String = #0;
integer (void [] buf, integer len) Hash_Buffer = #0;
(void [][]) (hashtab_t tab) Hash_GetList = #0;
void (hashtab_t tab) Hash_Stats = #0;
