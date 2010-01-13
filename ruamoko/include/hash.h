#ifndef __ruamoko_hash_h
#define __ruamoko_hash_h

struct _hashtab_t {};
typedef struct _hashtab_t [] hashtab_t;

@extern hashtab_t (integer size, string (void []ele, void []data) gk, void (void []ele, void []data) f, void [] ud) Hash_NewTable;
@extern void (hashtab_t tab, unsigned (void []ele, void []data) gh, integer (void [] ele1, void [] ele2, void [] data) cmp) Hash_SetHashCompare;
@extern void (hashtab_t tab) Hash_DelTable;
@extern void (hashtab_t tab) Hash_FlushTable;
@extern integer (hashtab_t tab, void [] ele) Hash_Add;
@extern integer (hashtab_t tab, void [] ele) Hash_AddElement;
@extern (void []) (hashtab_t tab, string key) Hash_Find;
@extern (void []) (hashtab_t tab, void [] ele) Hash_FindElement;
@extern (void [][]) (hashtab_t tab, string key) Hash_FindList;
@extern (void [][]) (hashtab_t tab, void [] ele) Hash_FindElementList;
@extern (void []) (hashtab_t tab, string key) Hash_Del;
@extern (void []) (hashtab_t tab, void [] ele) Hash_DelElement;
@extern void (hashtab_t tab, void [] ele) Hash_Free;
@extern integer (string str) Hash_String;
@extern integer (void [] buf, integer len) Hash_Buffer;
@extern (void [][]) (hashtab_t tab) Hash_GetList;
@extern void (hashtab_t tab) Hash_Stats;

#endif	// __ruamoko_hash_h
