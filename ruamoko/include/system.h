#ifndef __ruamoko_system_h
#define __ruamoko_system_h

@extern float time;

@extern void (string s) precache_sound;
@extern void (string s) precache_model;
@extern void (entity client, string s) stuffcmd;
@extern float (string s) cvar;
@extern void (string s) localcmd;
@extern void (string s) changelevel;
@extern void (string var, string val) cvar_set;
@extern string (string s) precache_file;
@extern string (string s) precache_model2;
@extern string (string s) precache_sound2;
@extern string (string s) precache_file2;
@extern float () checkextension;
@extern string () gametype;

#endif//__ruamoko_system_h
