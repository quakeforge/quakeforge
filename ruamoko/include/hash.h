#ifndef __ruamoko_hash_h
#define __ruamoko_hash_h

struct _hashtab_t = {};
typedef _hashtab_t [] hashtab_t;

hashtab_t () Hash_NewTable;
void () Hash_SetHashCompare;
void () Hash_DelTable;
void () Hash_FlushTable;
integer () Hash_Add;
integer () Hash_AddElement;
(void []) () Hash_Find;
(void []) () Hash_FindElement;
(void [][]) () Hash_FindList;
(void [][]) () Hash_FindElementList;
(void []) () Hash_Del;
(void []) () Hash_DelElement;
void () Hash_Free;
integer () Hash_String;
integer () Hash_Buffer;
(void [][]) () Hash_GetList;
void () Hash_Stats;

#endif __ruamoko_hash_h
