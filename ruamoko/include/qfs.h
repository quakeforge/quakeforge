#ifndef __ruamoko_qfs_h
#define __ruamoko_qfs_h

#include "qfile.h"

struct _qfslist_t {
	integer count;
	string *list;
};
typedef struct _qfslist_t *QFSlist;

@extern QFile QFS_Open (string path, string mode);
@extern QFile QFS_WOpen (string path, integer zip);
@extern integer QFS_Rename (string old, string new);
@extern void *QFS_LoadFile (string filename);
@extern QFile QFS_OpenFile (string filename);
@extern integer QFS_WriteFile (string filename, void *buf, integer count);
@extern QFSlist QFS_Filelist (string path, string ext, integer strip);
@extern void  QFS_FilelistFree (QFSlist list);

#endif//__ruamoko_qfs_h
