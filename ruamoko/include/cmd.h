#ifndef __ruamoko_cmd_h
#define __ruamoko_cmd_h

@extern void (string name, void () func) Cmd_AddCommand;
@extern integer () Cmd_Argc;
@extern string (integer arg) Cmd_Argv;
@extern string (integer arg) Cmd_Args;

#endif//__ruamoko_cmd_h
