#ifndef __ruamoko_qfs_h
#define __ruamoko_qfs_h

#include <qfile.h>

struct _qfslist_t {
	int count;
	string *list;
};
typedef struct _qfslist_t *QFSlist;

@extern QFile QFS_Open (string path, string mode);
@extern QFile QFS_WOpen (string path, int zip);
@extern int QFS_Rename (string old, string new);
@extern void *QFS_LoadFile (string filename);
@extern QFile QFS_OpenFile (string filename);
@extern int QFS_WriteFile (string filename, void *buf, int count);
@extern QFSlist QFS_Filelist (string path, string ext, int strip);
@extern void  QFS_FilelistFree (QFSlist list);
@extern string QFS_GetDirectory (void);

#endif//__ruamoko_qfs_h
