#ifndef __ruamoko_cvar_h
#define __ruamoko_cvar_h

@extern void (string varname, string value) Cvar_SetString;
@extern void (string varname, float value) Cvar_SetFloat;
@extern void (string varname, integer value) Cvar_SetInteger;
@extern void (string varname, vector value) Cvar_SetVector;
@extern string (string varname) Cvar_GetString;
@extern float (string varname) Cvar_GetFloat;
@extern integer (string varname) Cvar_GetInteger;
@extern vector (string varname) Cvar_GetVector;
@extern void (string varname) Cvar_Toggle;

#endif//__ruamoko_cvar_h
