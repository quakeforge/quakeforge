#include "file.h"

file_t (string path, string mode) File_Open = #0;
void (file_t file) File_Close = #0;
string (file_t file) File_GetLine = #0;
