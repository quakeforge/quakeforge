#ifndef __ruamoko_string_h
#define __ruamoko_string_h

@extern void (entity client, string s) sprint;
@extern string (float f) ftos;
@extern string (vector v) vtos;
@extern float (string s) stof;
@extern float (string s) strlen;
@extern float (string goal, string s) charcount;
@extern string (...) sprintf;
@extern string (integer i) itos;
@extern integer (string s) stoi;
@extern vector (string s) stov;

#endif//__ruamoko_string_h
