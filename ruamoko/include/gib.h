#ifndef __ruamoko_gib_h
#define __ruamoko_gib_h

@extern void (string name, void (integer argc, string [] argv) func) GIB_Builtin_Add;
@extern integer (string value) GIB_Return;
@extern integer (void) GIB_Handle_Class_New = #0;
@extern integer (void [] data, integer class) GIB_Handle_New = #0;
@extern void [] (integer handle, integer class) GIB_Handle_Get = #0;
@extern void (integer handle, integer class) GIB_Handle_Free = #0;

#endif//__ruamoko_gib_h
