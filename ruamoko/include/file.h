#ifndef __ruamoko_file_h
#define __ruamoko_file_h

struct _file_t = {};
typedef _file_t [] file_t;

@extern file_t (string path, string mode) File_Open;
@extern void (file_t file) File_Close;
@extern string (file_t file) File_GetLine;

#endif//__ruamoko_file_h
