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

string QFS_FileBase (string path) = #0;
string QFS_FileExtension (string path) = #0;
string QFS_DefaultExtension (string path, string ext) = #0;
string QFS_SetExtension (string path, string ext) = #0;
string QFS_StripExtension (string path) = #0;
string QFS_CompressPath (string path) = #0;
string QFS_SkipPath (string path) = #0;
