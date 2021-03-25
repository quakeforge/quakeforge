#include <hash.h>

hashtab_t *Hash_NewTable (int size, string gk (void *ele, void *data), void f (void *ele, void *data), void *ud) = #0;
void Hash_SetHashCompare (hashtab_t *tab, unsigned gh (void *ele, void *data), int cmp (void *ele1, void *ele2, void *data)) = #0;
void Hash_DelTable (hashtab_t *tab) = #0;
void Hash_FlushTable (hashtab_t *tab) = #0;
int Hash_Add (hashtab_t *tab, void *ele) = #0;
int Hash_AddElement (hashtab_t *tab, void *ele) = #0;
void *Hash_Find (hashtab_t *tab, string key) = #0;
void *Hash_FindElement (hashtab_t *tab, void *ele) = #0;
void **Hash_FindList (hashtab_t *tab, string key) = #0;
void **Hash_FindElementList (hashtab_t *tab, void *ele) = #0;
void *Hash_Del (hashtab_t *tab, string key) = #0;
void *Hash_DelElement (hashtab_t *tab, void *ele) = #0;
void Hash_Free (hashtab_t *tab, void *ele) = #0;
int Hash_String (string str) = #0;
int Hash_Buffer (void *buf, int len) = #0;
void **Hash_GetList (hashtab_t *tab) = #0;
void Hash_Stats (hashtab_t *tab) = #0;
