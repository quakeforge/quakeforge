#ifndef __cl_console_h
#define __cl_console_h

extern struct console_data_s con_data;
extern bool con_debug;

void Con_Debug_Init (void);
void Con_Debug_InitCvars (void);
void Con_Debug_Shutdown (void);
void Con_Debug_Draw (void);
void Con_Show_Mouse (bool visible);

#endif//__cl_console_h
