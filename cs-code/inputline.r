#include "inputline.h"

inputline_t (integer lines, integer size, integer prompt) InputLine_Create = #0;
void (inputline_t il, integer width) InputLine_SetWidth = #0;
void (inputline_t il) InputLine_Destroy = #0;
void (inputline_t il) InputLine_Clear = #0;
void (inputline_t il, integer ch) InputLine_Process = #0;
void (inputline_t il, integer x, integer y, integer cursor) InputLine_Draw = #0;
void (inputline_t il, string str) InputLine_SetText = #0;
string (inputline_t il) InputLine_GetText = #0;
