#ifndef __ruamoko_server_h
#define __ruamoko_server_h

@extern string precache_sound (string s);
@extern string precache_model (string s);
@extern void stuffcmd (entity client, string s);
@extern void localcmd (string s);
@extern void changelevel (string s);
@extern string precache_file (string s);
@extern string precache_model2 (string s);
@extern string precache_sound2 (string s);
@extern string precache_file2 (string s);
@extern float checkextension (void);

#endif//__ruamoko_server_h
