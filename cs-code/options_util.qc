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
	local float perc;
	local integer i;
	if(perc_val > 100) {
		perc_val = 100;
	} else if(perc_val < 0) {
		perc_val = 0;
	}
	perc = itof(perc_val) / (100/itof(size));
	Draw_String (x, y, "[");
	for (i = 0; i <= size; i++) {
		Draw_String (x + ((i+1)*8), y, "-");
	}
	Draw_String (x + ((i+1)*8), y, "]");
    Draw_Character (x + ((ftoi(perc) + 1) * 8), y, '*');
};

void (integer x, integer y, integer spacing, string spacechar, string label, string valstr) draw_item =
{
	local integer i;
	Draw_String	(x,  y,	label); 
	for (i = x + String_Len(label)*8; i < (x+spacing); i+=8) {
		Draw_String (i, y, spacechar);
	}
	Draw_String (x + spacing,  y, valstr);
};
void (integer x, integer y, integer spacing, string label, string valstr) draw_val_item =
{
	draw_item (x, y, spacing, ".", label, ":" + valstr);
};

integer (float min, float max, float val) to_percentage =
{
	local integer perc;
	local float max_v = (max-min);
	val -= min;

	if(val > max_v) {
		val = max_v;
	}
	if(val < 0) {
		val = 0;
	}
	perc = ftoi((val/max_v) * 100);

	return perc;
};

float (float min, float max, float step, float val, integer cntflag) min_max_cnt =
{
	if(cntflag) {
		val += step;
	} else {
		val -= step;
	}

	if(val > max) {
		val = min;
	} else if(val < min) {
		val = max;
	}
	return val;
};
