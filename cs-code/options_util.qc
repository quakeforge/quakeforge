float time;
void (integer x, integer y) opt_cursor =
{
	Draw_Character (x, y, 12 + (integer (time * 4) & 1));
};

string (string cvarstr) get_cvar_state =
{
	if(cvar(cvarstr)) {
		return("On");
	} else {
		return("Off");
	}
};

void (integer x, integer y, integer size, integer perc_val) draw_perc_bar =
{
	local integer perc, i;
	if(perc_val > 100) {
		perc_val = 100;
	} else if(perc_val < 0) {
		perc_val = 0;
	}
	perc = perc_val / (100/size);
	Draw_String (x, y, "[");
	for (i = 0; i <= (size+1); i++) {
		Draw_String (x + ((i+1)*8), y, "-");
	}
	Draw_String (x + ((i+1)*8), y, "]");
    Draw_Character (x + ((perc + 1) * 8), y, '*');
};
