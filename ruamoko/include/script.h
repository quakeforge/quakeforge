#ifndef __ruamoko_script_h
#define __ruamoko_script_h

typedef @handle rua_script script_t;

@extern script_t Script_New (void);
@extern void Script_Delete (script_t script);
// returns the token string
@extern string Script_Start (script_t script, string file, string data);
@extern string Script_FromFile (script_t script, string filename, @handle _qfile_t file);
@extern int Script_TokenAvailable (script_t script, int crossline);
@extern int Script_GetToken (script_t script, int crossline);
@extern void Script_UngetToken (script_t script);
@extern string Script_Error (script_t script);
@extern int Script_NoQuoteLines (script_t script);
@extern void Script_SetSingle (script_t script, string single);
@extern int Script_GetLine (script_t script);

#endif//__ruamoko_script_h
