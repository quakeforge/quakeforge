#include "QF/keys.h"
string (integer target, integer keynum, string binding) Key_SetBinding = #0;
integer (integer target, integer bindnum, string binding) Key_LookupBinding = #0;
integer (integer target, string binding) Key_CountBinding = #0;
string (integer keynum) Key_KeynumToString = #0;
