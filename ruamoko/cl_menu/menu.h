#ifndef __menu_h
#define __menu_h

@extern void Menu_Begin (int x, int y, string text);
@extern void Menu_FadeScreen (int val);
@extern void Menu_Draw (int (func)(int x, int y));
@extern void Menu_EnterHook (void (func)(void));
@extern void Menu_LeaveHook (void (func)(void));
@extern void Menu_Pic (int x, int y, string name);
@extern void Menu_SubPic (int x, int y, string name, int srcx, int srcy,
						  int width, int height);
@extern void Menu_CenterPic (int x, int y, string name);
@extern void Menu_CenterSubPic (int x, int y, string name, int srcx, int srcy,
								int width, int height);
@extern void Menu_Item (int x, int y, string text,
						int (func)(string text, int key),
						int allkeys);
@extern void Menu_Cursor (void (func)(int x, int y));
@extern void Menu_KeyEvent (bool (func)(int key, int unicode, bool down));
@extern void Menu_End (void);
@extern void Menu_TopMenu (string name);
@extern void Menu_SelectMenu (string name);
@extern void Menu_SetQuit (int (func)(void));
@extern void Menu_Quit (void);
@extern int  Menu_GetIndex (void);
@extern void Menu_Next (void);
@extern void Menu_Prev (void);
@extern void Menu_Enter (void);

#endif//__menu_h;
