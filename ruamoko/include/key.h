#ifndef __ruamoko_key_h
#define __ruamoko_key_h

#include <QF/keys.h>

@extern int Key_keydown (int keynum);
@extern string Key_SetBinding (string imt, int keynum, string binding);
@extern int Key_LookupBinding (string imt, int bindnum, string binding);
@extern int Key_CountBinding (string imt, string binding);
@extern string Key_KeynumToString (int keynum);

#endif//__ruamoko_key_h
