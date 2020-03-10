#include <string.h>

string (float f) ftos = #26;
string (vector v) vtos = #27;
float (string s) stof = #81;
float (string s) strlen = #0x000f0000 + 100;
float (string goal, string s) charcount = #0x000f0000 + 101;
string (string fmt, ...) sprintf = #0x000f0000 + 109;
string vsprintf (string fmt, @va_list args) = #0;
string (int i) itos = #0x000f0000 + 112;
int (string s) stoi = #0x000f0000 + 113;
vector (string s) stov = #0x000f0000 + 114;

string (void) str_new = #0;
string (string str) str_free = #0;
string (string dst, string src) str_copy = #0;
string (string dst, string src) str_cat = #0;
string (string str) str_clear = #0;
string (string str, int start) str_mid = #0;
string (string str, int start, int len) str_mid = #0;
string (string haystack, string needle) str_str = #0;
