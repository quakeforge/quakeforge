#define NUM_CON_TIMES 4
float [NUM_CON_TIMES] con_times;

integer con_state;
integer [5] key_focus;
integer force_console;
string status_bar;

void ()
Con_ClearNotify =
{
	local integer i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
};

void ()
Con_ToggleConsole_f =
{
	if (force_console)
		con_state = 1;
	else
		con_state ^= 1;
	Key_SetFocus (key_focus[con_state]);
	Con_ClearNotify ();
};

void ()
Con_ToggleChat_f =
{
	if (force_console)
		con_state = 1;
	else
		con_state ^= 2;
	Key_SetFocus (key_focus[con_state]);
	Con_ClearNotify ();
};

void ()
Con_Clear_f =
{
};

void ()
Con_MessageMode_f =
{
	if (force_console)
		return;
	chat_team = false;
	Key_SetFocus (key_focus[4]);
};

void ()
Con_MessageMode2_f =
{
	if (force_console)
		return;
	chat_team = true;
	Key_SetFocus (key_focus[4]);
};

void ()
Con_Resize =
{
};

void ()
Con_DrawInput =
{
};

void ()
Con_DrawNotify =
{
};

void (integer lines)
Con_DrawConsole =
{
};

void (integer lines)
Con_DrawDownload =
{
	strset (bar, 0, '\x80');
	strfill (bar, 1, con_linewidth - 2, '\x81');
	strset (bar, 0, '\x80');
	strset (bar, con_linewidth - 1, '\x82');
};
