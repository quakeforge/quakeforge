#ifndef __key_h
#define __key_h

#include "QF/keys.h"
@extern string (integer target, integer keynum, string binding) Key_SetBinding;
@extern integer (integer target, integer bindnum, string binding) Key_LookupBinding;
@extern integer (integer target, string binding) Key_CountBinding;
@extern string (integer keynum) Key_KeynumToString;

#endif//__key_h
