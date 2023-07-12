#include <qfile.h>
#include <script.h>

script_t Script_New (void) = #0;
void Script_Delete (script_t script) = #0;
// returns the token string
string Script_Start (script_t script, string file, string data) = #0;
string Script_FromFile (script_t script, string filename, QFile file) = #0;
int Script_TokenAvailable (script_t script, int crossline) = #0;
int Script_GetToken (script_t script, int crossline) = #0;
void Script_UngetToken (script_t script) = #0;
string Script_Error (script_t script) = #0;
int Script_NoQuoteLines (script_t script) = #0;
void Script_SetSingle (script_t script, string single) = #0;
int Script_GetLine (script_t script) = #0;
