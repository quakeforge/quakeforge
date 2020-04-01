#include <string.h>

int (string s) strlen = #0;
string (string fmt, ...) sprintf = #0;
string vsprintf (string fmt, @va_list args) = #0;
string (void) str_new = #0;
void (string str) str_free = #0;
string str_hold (string str) = #0;
int str_valid (string str) = #0;
int str_mutable (string str) = #0;
string (string dst, string src) str_copy = #0;
string (string dst, string src) str_cat = #0;
string (string str) str_clear = #0;
string (string str, int start) str_mid = #0;
string (string str, int start, int len) str_mid = #0;
string (string haystack, string needle) str_str = #0;
int str_char (string str, int ind) = #0;
string str_quote (string str) = #0;
