#ifndef __ruamoko_hash_h
#define __ruamoko_hash_h

typedef struct _hashtab_t hashtab_t;

@extern hashtab_t *Hash_NewTable (int size, string gk (void *ele, void *data), void f (void *ele, void *data), void *ud);
@extern void Hash_SetHashCompare (hashtab_t *tab, unsigned gh (void *ele, void *data), int cmp (void *ele1, void *ele2, void *data));
@extern void Hash_DelTable (hashtab_t *tab);
@extern void Hash_FlushTable (hashtab_t *tab);
@extern int Hash_Add (hashtab_t *tab, void *ele);
@extern int Hash_AddElement (hashtab_t *tab, void *ele);
@extern void *Hash_Find (hashtab_t *tab, string key);
@extern void *Hash_FindElement (hashtab_t *tab, void *ele);
@extern void **Hash_FindList (hashtab_t *tab, string key);
@extern void **Hash_FindElementList (hashtab_t *tab, void *ele);
@extern void *Hash_Del (hashtab_t *tab, string key);
@extern void *Hash_DelElement (hashtab_t *tab, void *ele);
@extern void Hash_Free (hashtab_t *tab, void *ele);
@extern int Hash_String (string str);
@extern int Hash_Buffer (void *buf, int len);
@extern void **Hash_GetList (hashtab_t *tab);
@extern void Hash_Stats (hashtab_t *tab);

#endif	// __ruamoko_hash_h
