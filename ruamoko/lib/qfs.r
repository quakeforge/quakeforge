#include "qfs.h"

QFile QFS_Open (string path, string mode) = #0;
QFile QFS_WOpen (string path, integer zip) = #0;
integer QFS_Rename (string old, string new) = #0;
void *QFS_LoadFile (string filename) = #0;
QFile QFS_OpenFile (string filename) = #0;
integer QFS_WriteFile (string filename, void *buf, integer count) = #0;
QFSlist QFS_Filelist (string path, string ext, integer strip) = #0;
void  QFS_FilelistFree (QFSlist list) = #0;
