#include "menu.h"

void (integer x, integer y, string text) Menu_Begin = #0;
void (integer val) Menu_FadeScreen = #0;
void (integer (integer x, integer y) func) Menu_Draw = #0;
void (void () func) Menu_EnterHook = #0;
void (void () func) Menu_LeaveHook = #0;
void (integer x, integer y, string name) Menu_Pic = #0;
void (integer x, integer y, string name, integer srcx, integer srcy, integer width, integer height) Menu_SubPic = #0;
void (integer x, integer y, string name) Menu_CenterPic = #0;
void (integer x, integer y, string name, integer srcx, integer srcy, integer width, integer height) Menu_CenterSubPic = #0;
void (integer x, integer y, string text, integer (string text, integer key) func, integer allkeys) Menu_Item = #0;
void (void (integer x, integer y) func) Menu_Cursor = #0;
void (integer (integer key, integer unicode, integer down) func) Menu_KeyEvent = #0;
void () Menu_End = #0;
void (string name) Menu_TopMenu = #0;
void (string name) Menu_SelectMenu = #0;
void (integer () func) Menu_SetQuit = #0;
void () Menu_Quit = #0;
integer () Menu_GetIndex = #0;
