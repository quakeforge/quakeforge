#include <qfs.h>

QFile QFS_Open (string path, string mode) = #0;
QFile QFS_WOpen (string path, int zip) = #0;
int QFS_Rename (string old, string new) = #0;
void *QFS_LoadFile (string filename) = #0;
QFile QFS_OpenFile (string filename) = #0;
int QFS_WriteFile (string filename, void *buf, int count) = #0;
QFSlist QFS_Filelist (string path, string ext, int strip) = #0;
void  QFS_FilelistFree (QFSlist list) = #0;
string QFS_GetDirectory (void) = #0;
