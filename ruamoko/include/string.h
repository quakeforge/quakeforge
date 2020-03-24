#ifndef __ruamoko_string_h
#define __ruamoko_string_h

@extern int strlen (string s);
@extern string sprintf (string fmt, ...);
@extern string vsprintf (string fmt, @va_list args);
@extern string str_new (void);
@extern string str_free (string str);
@extern string str_copy (string dst, string src);
@extern string str_cat (string dst, string src);
@extern string str_clear (string str);
@extern @overload string str_mid (string str, int start);
@extern @overload string str_mid (string str, int start, int len);
@extern string str_str (string haystack, string needle);
@extern int str_char (string str, int ind);

#endif//__ruamoko_string_h
