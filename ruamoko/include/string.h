#ifndef __ruamoko_string_h
#define __ruamoko_string_h

@extern int strlen (string s);
@extern string sprintf (string fmt, ...);
@extern string vsprintf (string fmt, @va_list args);
@extern string str_new (void);
@extern string str_unmutable (string str);
@extern void str_free (string str);
@extern string str_hold (string str);
@extern int str_valid (string str);
@extern int str_mutable (string str);
@extern string str_copy (string dst, string src);
@extern string str_cat (string dst, string src);
@extern string str_clear (string str);
@extern @overload string str_mid (string str, int start);
@extern @overload string str_mid (string str, int start, int len);
int str_str (string haystack, string needle);
@extern int str_char (string str, int ind);
string str_quote (string str);
string str_lower (string str);
string str_upper (string str);
double strtod (string str, int *end);
float strtof (string str, int *end);
long strtol (string str, int *end, int base);
unsigned long strtoul (string str, int *end, int base);

#endif//__ruamoko_string_h
