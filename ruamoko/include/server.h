#ifndef __ruamoko_server_h
#define __ruamoko_server_h

@extern void (string s) precache_sound;
@extern void (string s) precache_model;
@extern void (entity client, string s) stuffcmd;
@extern void (string s) localcmd;
@extern void (string s) changelevel;
@extern string (string s) precache_file;
@extern string (string s) precache_model2;
@extern string (string s) precache_sound2;
@extern string (string s) precache_file2;
@extern float () checkextension;

#endif//__ruamoko_server_h
