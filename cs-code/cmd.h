#ifndef __cmd_h
#define __cmd_h

@extern void (string name, void () func) Cmd_AddCommand;
@extern integer () Cmd_Argc;
@extern string (integer arg) Cmd_Argv;
@extern string (integer arg) Cmd_Args;
@extern string (integer arg) Cmd_Argu;
@extern void (string value) Cmd_Return;

#endif//__cmd_h
