#ifndef __ruamoko_string_h
#define __ruamoko_string_h

@extern string ftos (float f);
@extern string vtos (vector v);
@extern float stof (string s);
@extern float strlen (string s);
@extern float charcount (string goal, string s);
@extern string sprintf (string fmt, ...);
@extern string vsprintf (string fmt, @va_list args);
@extern string itos (int i);
@extern int stoi (string s);
@extern vector stov (string s);

@extern string str_new (void);
@extern string str_free (string str);
@extern string str_copy (string dst, string src);
@extern string str_cat (string dst, string src);
@extern string str_clear (string str);
@extern @overload string str_mid (string str, int start);
@extern @overload string str_mid (string str, int start, int len);
@extern string str_str (string haystack, string needle);

#endif//__ruamoko_string_h
