#include "key.h"

string (int target, int keynum, string binding) Key_SetBinding = #0;
int (int target, int bindnum, string binding) Key_LookupBinding = #0;
int (int target, string binding) Key_CountBinding = #0;
string (int keynum) Key_KeynumToString = #0;
