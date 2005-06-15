#ifndef __ruamoko_string_h
#define __ruamoko_string_h

@extern string (float f) ftos;
@extern string (vector v) vtos;
@extern float (string s) stof;
@extern float (string s) strlen;
@extern float (string goal, string s) charcount;
@extern string (...) sprintf;
@extern string (integer i) itos;
@extern integer (string s) stoi;
@extern vector (string s) stov;

@extern string (void) str_new;
@extern string (string str) str_free;
@extern string (string dst, string src) str_copy;
@extern string (string dst, string src) str_cat;
@extern string (string str) str_clear;
@extern string (string str, integer start) @overload str_mid;
@extern string (string str, integer start, integer len) @overload str_mid;
@extern string (string haystack, string needle) str_str;

#endif//__ruamoko_string_h
