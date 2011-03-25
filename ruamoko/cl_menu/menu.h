#ifndef __menu_h
#define __menu_h

@extern void (int x, int y, string text) Menu_Begin;
@extern void (int val) Menu_FadeScreen;
@extern void (int (int x, int y) func) Menu_Draw;
@extern void (void () func) Menu_EnterHook;
@extern void (void () func) Menu_LeaveHook;
@extern void (int x, int y, string name) Menu_Pic;
@extern void (int x, int y, string name, int srcx, int srcy,
			  int width, int height) Menu_SubPic;
@extern void (int x, int y, string name) Menu_CenterPic;
@extern void (int x, int y, string name, int srcx, int srcy,
			  int width, int height) Menu_CenterSubPic;
@extern void (int x, int y, string text,
			  int (string text, int key) func,
			  int allkeys) Menu_Item;
@extern void (void (int x, int y) func) Menu_Cursor;
@extern void (int (int key, int unicode, int down)
			  func) Menu_KeyEvent;
@extern void () Menu_End;
@extern void (string name) Menu_TopMenu;
@extern void (string name) Menu_SelectMenu;
@extern void (int () func) Menu_SetQuit;
@extern void () Menu_Quit;
@extern int () Menu_GetIndex;

#endif//__menu_h;
