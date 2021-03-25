#include <cvar.h>

float (string s) cvar = #45;
void (string var, string val) cvar_set = #72;

int Cvar_MakeAlias (string alias_name, string cvar_name) = #0;
int Cvar_RemoveAlias (string alias_name) = #0;
void (string varname, string value) Cvar_SetString = #0;
void (string varname, float value) Cvar_SetFloat = #0;
void (string varname, int value) Cvar_SetInteger = #0;
void (string varname, vector value) Cvar_SetVector = #0;
string (string varname) Cvar_GetString = #0;
float (string varname) Cvar_GetFloat = #0;
int (string varname) Cvar_GetInteger = #0;
vector (string varname) Cvar_GetVector = #0;
void (string varname) Cvar_Toggle = #0;
