#ifndef __ruamoko_cmd_h
#define __ruamoko_cmd_h

@extern void Cmd_AddCommand (string name, void func ());
@extern int Cmd_Argc (void);
@extern string Cmd_Argv (int arg);
@extern string Cmd_Args (int arg);

#endif//__ruamoko_cmd_h
