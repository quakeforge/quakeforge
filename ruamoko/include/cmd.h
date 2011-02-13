#ifndef __ruamoko_cmd_h
#define __ruamoko_cmd_h

@extern void Cmd_AddCommand (string name, void func ());
@extern integer Cmd_Argc (void);
@extern string Cmd_Argv (integer arg);
@extern string Cmd_Args (integer arg);

#endif//__ruamoko_cmd_h
