#ifndef __ruamoko_hash_h
#define __ruamoko_hash_h

struct _hashtab_t {};
typedef struct _hashtab_t [] hashtab_t;

@extern hashtab_t Hash_NewTable (integer size, string (void []ele, void []data) gk, void (void []ele, void []data) f, void [] ud);
@extern void Hash_SetHashCompare (hashtab_t tab, unsigned (void []ele, void []data) gh, integer (void [] ele1, void [] ele2, void [] data) cmp);
@extern void Hash_DelTable (hashtab_t tab);
@extern void Hash_FlushTable (hashtab_t tab);
@extern integer Hash_Add (hashtab_t tab, void [] ele);
@extern integer Hash_AddElement (hashtab_t tab, void [] ele);
@extern (void []) Hash_Find (hashtab_t tab, string key);
@extern (void []) Hash_FindElement (hashtab_t tab, void [] ele);
@extern (void [][]) Hash_FindList (hashtab_t tab, string key);
@extern (void [][]) Hash_FindElementList (hashtab_t tab, void [] ele);
@extern (void []) Hash_Del (hashtab_t tab, string key);
@extern (void []) Hash_DelElement (hashtab_t tab, void [] ele);
@extern void Hash_Free (hashtab_t tab, void [] ele);
@extern integer Hash_String (string str);
@extern integer Hash_Buffer (void [] buf, integer len);
@extern (void [][]) Hash_GetList (hashtab_t tab);
@extern void Hash_Stats (hashtab_t tab);

#endif	// __ruamoko_hash_h
