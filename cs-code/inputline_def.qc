struct _inputline_t = {};	// opaque type :)
typedef _inputline_t [] inputline_t;
inputline_t (integer lines, integer width, integer prompt) InputLine_Create = #0;
void (inputline_t il) InputLine_Destroy = #0;
void (inputline_t il) InputLine_Clear = #0;
void (inputline_t il, integer ch) InputLine_Process = #0;
void (inputline_t il, integer x, integer y, integer cursor) InputLine_Draw = #0;
