#ifndef __file_h
#define __file_h

//FIXME need a proper file struct, string sucks
@extern string (string path, string mode) File_Open;
@extern void (string file) File_Close;
@extern string (string file) File_GetLine;

#endif//__file_h
