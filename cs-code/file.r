#include "file.h"

//FIXME need a proper file struct, string sucks
string (string path, string mode) File_Open = #0;
void (string file) File_Close = #0;
string (string file) File_GetLine = #0;
