#include "qfs.h"

integer (string old, string new) QFS_Rename = #0;
(void []) (string filename) QFS_LoadFile = #0;
QFile (string filename) QFS_OpenFile = #0;
integer (string filename, void [] buf, integer count) QFS_WriteFile = #0;
QFSlist (string path, string ext, integer strip) QFS_Filelist = #0;
void  (QFSlist list) QFS_FilelistFree = #0;
