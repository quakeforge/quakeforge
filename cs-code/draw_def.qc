struct _qpic_t = {
	integer width;
	integer height;
};
typedef _qpic_t [] qpic_t;

qpic_t (string name, integer alpha) Draw_CachePic = #0;

void (integer x, integer y, qpic_t pic) Draw_Pic = #0;
void (integer x, integer y, qpic_t pic) Draw_CenterPic = #0;

void (integer x, integer y, integer chr) Draw_Character = #0;
void (integer x, integer y, string text) Draw_String = #0;
void (integer x, integer y, string text, integer n) Draw_nString = #0;
void (integer x, integer y, string text) Draw_AltString = #0;
void (integer x, integer y, integer w, integer h, integer c) Draw_Fill = #0;
