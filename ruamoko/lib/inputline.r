#include "inputline.h"

inputline_t (integer lines, integer size, integer prompt) InputLine_Create = #0;
void (inputline_t il, void [] data) InputLine_SetUserData = 0;
void (inputline_t il, integer width) InputLine_SetWidth = #0;
void (inputline_t il) InputLine_Destroy = #0;
void (inputline_t il) InputLine_Clear = #0;
void (inputline_t il, integer ch) InputLine_Process = #0;
void (inputline_t il, integer cursor) InputLine_Draw = #0;
void (inputline_t il, string str) InputLine_SetText = #0;
string (inputline_t il) InputLine_GetText = #0;

@implementation InputLine

-free
{
	[at free];
	InputLine_Destroy (il);
	return [super free];
}

-initAt:(Point)p HistoryLines:(integer)l LineSize:(integer)s PromptChar:(integer)prompt
{
	[super init];
	id(at) = [[Point alloc] initWithPoint:p];
	il = InputLine_Create (l, s, prompt);
	InputLine_SetUserData (il, at);
	return self;
}

-setWidth:(integer)visibleWidth
{
	InputLine_SetWidth (il, visibleWidth);
	return self;
}

-process:(integer)key
{
	InputLine_Process (il, key);
	return self;
}

-draw:(BOOL)cursor
{
	InputLine_Draw (il, cursor);
}

-setText:(string)text
{
	InputLine_SetText (il, text);
	return self;
}

-(string)getText
{
	return InputLine_GetText (il);
}

@end
