#include "gib.h"

void (string name, void (integer argc, string [] argv) func) GIB_Builtin_Add = #0;
integer (string value) GIB_Return = #0;
integer (void) GIB_Handle_Class_New = #0;
integer (void [] data, integer class) GIB_Handle_New = #0;
void [] (integer handle, integer class) GIB_Handle_Get = #0;
void (integer handle, integer class) GIB_Handle_Free = #0;
