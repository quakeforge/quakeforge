#ifndef __string_h
#define __string_h

@extern string (integer old, integer new, string str) String_ReplaceChar;
@extern string (integer pos, integer len, string str) String_Cut;
@extern integer (string str) String_Len;
@extern integer (string str, integer pos) String_GetChar;

#endif
