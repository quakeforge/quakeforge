#ifndef __menu_h
#define __menu_h

@extern void (integer x, integer y, string text) Menu_Begin;
@extern void (integer val) Menu_FadeScreen;
@extern void (integer () func) Menu_Draw;
@extern void (integer () func) Menu_EnterHook;
@extern void (integer () func) Menu_LeaveHook;
@extern void (integer x, integer y, string name) Menu_Pic;
@extern void (integer x, integer y, string name, integer srcx, integer srcy, integer width, integer height) Menu_SubPic;
@extern void (integer x, integer y, string name) Menu_CenterPic;
@extern void (integer x, integer y, string name, integer srcx, integer srcy, integer width, integer height) Menu_CenterSubPic;
@extern void (integer x, integer y, string text, integer (string text, integer key) func, integer allkeys) Menu_Item;
@extern void (void (integer x, integer y) func) Menu_Cursor;
@extern void (integer (integer key, integer unicode, integer down) func) Menu_KeyEvent;
@extern void () Menu_End;
@extern void (string name) Menu_TopMenu;
@extern void (string name) Menu_SelectMenu;
@extern void (integer () func) Menu_SetQuit;
@extern void () Menu_Quit;
@extern integer () Menu_GetIndex;

@extern float () random;
@extern float () traceon; 
@extern float () traceoff;
@extern string () gametype;
@extern string (...) sprintf;

#endif//__menu_h;
