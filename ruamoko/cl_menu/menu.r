#include "menu.h"

void (int x, int y, string text) Menu_Begin = #0;
void (int val) Menu_FadeScreen = #0;
void (int (int x, int y) func) Menu_Draw = #0;
void (void () func) Menu_EnterHook = #0;
void (void () func) Menu_LeaveHook = #0;
void (int x, int y, string name) Menu_Pic = #0;
void (int x, int y, string name, int srcx, int srcy, int width, int height) Menu_SubPic = #0;
void (int x, int y, string name) Menu_CenterPic = #0;
void (int x, int y, string name, int srcx, int srcy, int width, int height) Menu_CenterSubPic = #0;
void (int x, int y, string text, int (string text, int key) func, int allkeys) Menu_Item = #0;
void (void (int x, int y) func) Menu_Cursor = #0;
void (bool (int key, int unicode, bool down) func) Menu_KeyEvent = #0;
void () Menu_End = #0;
void (string name) Menu_TopMenu = #0;
void (string name) Menu_SelectMenu = #0;
void (int () func) Menu_SetQuit = #0;
void () Menu_Quit = #0;
int () Menu_GetIndex = #0;
void (void) Menu_Next = #0;
void (void) Menu_Prev = #0;
void (void) Menu_Enter = #0;
void (void) Menu_Leave = #0;
