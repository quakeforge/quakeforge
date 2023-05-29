#include <string.h>

int (string s) strlen = #0;
string (string fmt, ...) sprintf = #0;
string vsprintf (string fmt, @va_list args) = #0;
string (void) str_new = #0;
string str_unmutable (string str) = #0;
void (string str) str_free = #0;
string str_hold (string str) = #0;
int str_valid (string str) = #0;
int str_mutable (string str) = #0;
string (string dst, string src) str_copy = #0;
string (string dst, string src) str_cat = #0;
string (string str) str_clear = #0;
string (string str, int start) str_mid = #0;
string (string str, int start, int len) str_mid = #0;
int str_str (string haystack, string needle) = #0;
int str_char (string str, int ind) = #0;
string str_quote (string str) = #0;
string str_lower (string str) = #0;
string str_upper (string str) = #0;
double strtod (string str, int *end) = #0;
float strtof (string str, int *end) = #0;
long strtol (string str, int *end, int base) = #0;
unsigned long strtoul (string str, int *end, int base) = #0;
