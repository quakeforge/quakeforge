#ifndef __ruamoko_hash_h
#define __ruamoko_hash_h

struct _hashtab_t = {};
typedef struct _hashtab_t [] hashtab_t;

@extern hashtab_t () Hash_NewTable;
@extern void () Hash_SetHashCompare;
@extern void () Hash_DelTable;
@extern void () Hash_FlushTable;
@extern integer () Hash_Add;
@extern integer () Hash_AddElement;
@extern (void []) () Hash_Find;
@extern (void []) () Hash_FindElement;
@extern (void [][]) () Hash_FindList;
@extern (void [][]) () Hash_FindElementList;
@extern (void []) () Hash_Del;
@extern (void []) () Hash_DelElement;
@extern void () Hash_Free;
@extern integer () Hash_String;
@extern integer () Hash_Buffer;
@extern (void [][]) () Hash_GetList;
@extern void () Hash_Stats;

#endif	// __ruamoko_hash_h
