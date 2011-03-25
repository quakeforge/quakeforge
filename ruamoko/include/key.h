#ifndef __ruamoko_key_h
#define __ruamoko_key_h

#include "QF/keys.h"
@extern string Key_SetBinding (int target, int keynum, string binding);
@extern int Key_LookupBinding (int target, int bindnum, string binding);
@extern int Key_CountBinding (int target, string binding);
@extern string Key_KeynumToString (int keynum);

#endif//__ruamoko_key_h
