#ifndef __ruamoko_key_h
#define __ruamoko_key_h

#include "QF/keys.h"
@extern string Key_SetBinding (integer target, integer keynum, string binding);
@extern integer Key_LookupBinding (integer target, integer bindnum, string binding);
@extern integer Key_CountBinding (integer target, string binding);
@extern string Key_KeynumToString (integer keynum);

#endif//__ruamoko_key_h
