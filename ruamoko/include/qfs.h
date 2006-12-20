#ifndef __ruamoko_qfs_h
#define __ruamoko_qfs_h

#include "qfile.h"

struct _qfslist_t = {};
typedef struct _qfslist_t [] QFSlist;

@extern QFile (string path, string mode) QFS_Open;
@extern QFile (string path, integer zip) QFS_WOpen;
@extern integer (string old, string new) QFS_Rename;
@extern (void []) (string filename) QFS_LoadFile;
@extern QFile (string filename) QFS_OpenFile;
@extern integer (string filename, void [] buf, integer count) QFS_WriteFile;
@extern QFSlist (string path, string ext, integer strip) QFS_Filelist;
@extern void  (QFSlist list) QFS_FilelistFree;

#endif//__ruamoko_qfs_h
