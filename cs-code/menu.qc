float () random = #0;
float () traceon = #0;
string () gametype = #0;
string (...) sprintf = #0;

float time;
entity self;
.float nextthink;
.float frame;
.void () think;

integer do_single_player;

string [6] dot = {
	"gfx/menudot1.lmp",
	"gfx/menudot2.lmp",
	"gfx/menudot3.lmp",
	"gfx/menudot4.lmp",
	"gfx/menudot5.lmp",
	"gfx/menudot6.lmp",
};

string [32] quitMessage = {
/*   .........1.........2.... */
	"  Are you gonna quit    ",
	"  this game just like   ",
	"   everything else?     ",
	"                        ",

	" Milord, methinks that  ",
	"   thou art a lowly     ",
	" quitter. Is this true? ",
	"                        ",

	" Do I need to bust your ",
	"  face open for trying  ",
	"        to quit?        ",
	"                        ",

	" Man, I oughta smack you",
	"   for trying to quit!  ",
	"     Press Y to get     ",
	"      smacked out.      ",

	" Press Y to quit like a ",
	"   big loser in life.   ",
	"  Press N to stay proud ",
	"    and successful!     ",

	"   If you press Y to    ",
	"  quit, I will summon   ",
	"  Satan all over your   ",
	"      hard drive!       ",

	"  Um, Asmodeus dislikes ",
	" his children trying to ",
	" quit. Press Y to return",
	"   to your Tinkertoys.  ",

	"  If you quit now, I'll ",
	"  throw a blanket-party ",
	"   for you next time!   ",
	"                        "
};
integer quit_index;

void (integer x, integer y) spinner =
{
	Draw_Pic (x, y, dot[integer(time * 10) % 6]);
};

void (integer x, integer y, integer width, integer lines) text_box =
{
	local integer cx, cy, n;
	local string p;

	cx = x;
	cy = y;
	Draw_Pic (cx, cy, "gfx/box_tl.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, "gfx/box_ml.lmp");
	}
	Draw_Pic (cx, cy + 8, "gfx/box_bl.lmp");

	cx += 8;
	while (width > 0) {
		cy = y;
		Draw_Pic (cx, cy, "gfx/box_tm.lmp");
		p = "gfx/box_mm.lmp";
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = "gfx/box_mm2.lmp";
			Draw_Pic (cx, cy, p);
		}
		Draw_Pic (cx, cy + 8, "gfx/box_bm.lmp");
		width -= 2;
		cx += 16;
	}

	cy = y;
	Draw_Pic (cx, cy, "gfx/box_tr.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, "gfx/box_mr.lmp");
	}
	Draw_Pic (cx, cy + 8, "gfx/box_br.lmp");
};

// ********* LOAD / SAVE

#define MAX_SAVEGAMES 12
string [MAX_SAVEGAMES] filenames;
integer [MAX_SAVEGAMES] loadable;
integer load_cursor;
integer save_cursor;

void () scan_saves =
{
	local integer i;
	local string f;	//FIXME need a file type;
	for (i = 0; i < MAX_SAVEGAMES; i++) {
		loadable[i] = 0;
		filenames[i] = "--- UNUSED SLOT ---";
		f = File_Open (sprintf ("s%i.sav", i), "rz");
		if (!f)
			continue;
		File_GetLine (f);
		filenames[i] = String_ReplaceChar ('_', ' ', File_GetLine (f));
		loadable[i] = 1;
		File_Close (f);
	}
};

integer (string text, integer key) load_f =
{
	scan_saves ();
	Menu_SelectMenu ("load");
	return 0;
};

integer (string text, integer key) save_f =
{
	scan_saves ();
	Menu_SelectMenu ("save");
	return 0;
};

integer () load_draw =
{
	local integer i;

	Draw_CenterPic (160, 4, "gfx/p_load.lmp");
	for (i=0 ; i< MAX_SAVEGAMES; i++)
		Draw_String (16, 32 + 8 * i, filenames[i]);
	Draw_Character (8, 32 + load_cursor * 8, 12 + (integer (time * 4) & 1));
	return 1;
};

integer () save_draw =
{
	local integer i;

	Draw_CenterPic (160, 4, "gfx/p_save.lmp");
	for (i=0 ; i< MAX_SAVEGAMES; i++)
		Draw_String (16, 32 + 8 * i, filenames[i]);
	Draw_Character (8, 32 + save_cursor * 8, 12 + (integer (time * 4) & 1));
	return 1;
};

integer (integer key, integer unicode, integer down) load_keyevent =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			load_cursor++;
			load_cursor %= MAX_SAVEGAMES;
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			load_cursor += MAX_SAVEGAMES - 1;
			load_cursor %= MAX_SAVEGAMES;
			return 1;
		case QFK_RETURN:
		case QFM_BUTTON1:
			if (loadable[load_cursor]) {
				Menu_SelectMenu (NIL);
				Cbuf_AddText (sprintf ("load s%i.sav\n", load_cursor));
			}
			return 1;
	}
	return 0;
};

integer (integer key, integer unicode, integer down) save_keyevent =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			save_cursor++;
			save_cursor %= MAX_SAVEGAMES;
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			save_cursor += MAX_SAVEGAMES - 1;
			save_cursor %= MAX_SAVEGAMES;
			return 1;
		case QFK_RETURN:
		case QFM_BUTTON1:
			Menu_SelectMenu (NIL);
			Cbuf_AddText (sprintf ("save s%i.sav\n", save_cursor));
			return 1;
	}
	return 0;
};

void () load_menu =
{
	Menu_Begin (0, 0, "load");
	Menu_FadeScreen (1);
	Menu_KeyEvent (load_keyevent);
	Menu_Draw (load_draw);
	Menu_End ();
};

void () save_menu =
{
	Menu_Begin (0, 0, "save");
	Menu_FadeScreen (1);
	Menu_KeyEvent (save_keyevent);
	Menu_Draw (save_draw);
	Menu_End ();
};

// ********* QUIT

integer () quit =
{
	Menu_SelectMenu ("quit");
	quit_index = integer (random () * 8);
	quit_index &= 7;
	return 0;
};

integer (string text, integer key) quit_f =
{
	quit ();
	return 0;
};

integer (integer key, integer unicode, integer down) quit_keyevent =
{
	if (key == 'y') {
		Menu_Quit ();
		return 1;
	}
	if (key == 'n') {
		Menu_SelectMenu (NIL);
		return 1;
	}
	return 0;
};

integer () quit_draw =
{
	text_box (56, 76, 24, 4);
	Draw_String (64, 84, quitMessage[quit_index *4 + 0]);
	Draw_String (64, 92, quitMessage[quit_index *4 + 1]);
	Draw_String (64, 100, quitMessage[quit_index *4 + 2]);
	Draw_String (64, 108, quitMessage[quit_index *4 + 3]);
	return 1;
};

void () quit_menu =
{
	Menu_Begin (0, 0, "quit");
	Menu_FadeScreen (1);
	Menu_KeyEvent (quit_keyevent);
	Menu_Draw (quit_draw);
	Menu_End ();
};

integer (string text, integer key) sp_start =
{
	Menu_SelectMenu (NIL);
	Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cbuf_AddText ("coop 0\n");
	Cbuf_AddText ("deathmatch 0\n");
	Cbuf_AddText ("teamplay 0\n");
	Cbuf_AddText ("listen 0\n");
	Cbuf_AddText ("noexit 0\n");
	Cbuf_AddText ("samelevel 0\n");
	Cbuf_AddText ("map start\n");
	return 0;
};

void () single_player_menu =
{
	Menu_Begin (54, 32, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_sgl.lmp");
	Menu_Pic (72, 32, "gfx/sp_menu.lmp");
	Menu_Cursor (spinner);
	Menu_Item (54, 32, "", sp_start, 0);
	Menu_Item (54, 52, "", load_f, 0);
	Menu_Item (54, 72, "", save_f, 0);
	Menu_End ();
};

// ********* MULTIPLAYER

integer JoiningGame;
integer lanConfig_cursor;
string my_tcpip_address;
string lanConfig_portname;
string lanConfig_joinname;
#define NUM_LANCONFIG_CMDS 3
integer [NUM_LANCONFIG_CMDS] lanConfig_cursor_table = { 72, 92, 124 };
inputline_t lanConfig_port_il;
inputline_t lanConfig_join_il;
inputline_t input_active;

integer () join_draw =
{
	local integer f = (320 - 26 * 8) / 2;
	text_box (f, 134, 24, 4);
	Draw_String (f, 142, " Commonly used to play  ");
	Draw_String (f, 150, " over the Internet, but ");
	Draw_String (f, 158, " also used on a Local   ");
	Draw_String (f, 166, " Area Network.          ");
	return 0;
};

integer () lanconfig_draw =
{
	local integer basex = 54;
	local string startJoin = JoiningGame ? "Join Game" : "New Game";
	local string protocol = "UDP";

	Draw_String (basex, 32, sprintf ("%s - %s", startJoin, protocol));
	basex += 8;
	Draw_String (basex, 52, "Address:");
	Draw_String (basex + 9 * 8, 52, "127.0.0.1");
	Draw_String (basex, lanConfig_cursor_table[0], "Port");
	text_box (basex + 8 * 8, lanConfig_cursor_table[0] - 8, 6, 1);
	InputLine_Draw (lanConfig_port_il, basex + 8 * 8, lanConfig_cursor_table[0], lanConfig_cursor == 0 && input_active);
	Draw_String (basex + 9 * 8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame) {
		Draw_String (basex, lanConfig_cursor_table[1], "Search for local games...");
		Draw_String (basex, 108, "Join game at:");
		text_box (basex + 8, lanConfig_cursor_table[2] - 8, 22, 1);
		InputLine_Draw (lanConfig_join_il, basex + 8, lanConfig_cursor_table[2], lanConfig_cursor == 2 && input_active);
		Draw_String (basex + 16, lanConfig_cursor_table[2], lanConfig_joinname);
	} else {
		text_box (basex, lanConfig_cursor_table[1] - 8, 2, 1);
		Draw_String (basex + 8, lanConfig_cursor_table[1], "OK");
	}
	if (!input_active)
		Draw_Character (basex - 8, lanConfig_cursor_table[lanConfig_cursor],
						12 + (integer (time * 4) & 1));

	return 0;
};

integer (integer key, integer unicode, integer down) lanconfig_keyevent =
{
	if (input_active)
		InputLine_Process (input_active, key >= 256 ? key : unicode);
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			if (!input_active) {
				lanConfig_cursor ++;
				lanConfig_cursor %= NUM_LANCONFIG_CMDS;
			}
			break;
		case QFK_UP:
		case QFM_WHEEL_UP:
			if (!input_active) {
				lanConfig_cursor += NUM_LANCONFIG_CMDS - 1;
				lanConfig_cursor %= NUM_LANCONFIG_CMDS;
			}
			break;
		case QFK_RETURN:
			if (input_active) {
				input_active = NIL;
			} else {
				if (lanConfig_cursor == 0) {
					input_active = lanConfig_port_il;
				} else if (JoiningGame) {
					if (lanConfig_cursor == 2) {
						input_active = lanConfig_join_il;
					}
				}
			}
			break;
	}
	return 1;
};

void () lanconfig_menu =
{
	Menu_Begin (54, 92, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Draw (lanconfig_draw);
	Menu_KeyEvent (lanconfig_keyevent);
	Menu_End ();
};

void () join_menu =
{
	Menu_Begin (54, 32, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Pic (72, 32, "gfx/dim_modm.lmp");
	Menu_Pic (72, 51, "gfx/dim_drct.lmp");
	Menu_Pic (72, 70, "gfx/dim_ipx.lmp");
	Menu_Pic (72, 89, "gfx/netmen4.lmp");
	lanconfig_menu ();
	Menu_Draw (join_draw);
	Menu_Cursor (spinner);
	Menu_End ();
};

integer (integer key, integer unicode, integer down) multi_player_keyevent =
{
	if (key == QFK_RETURN) {
		JoiningGame = (Menu_GetIndex () == 0);
	}
	return 0;
};

void () multi_player_menu =
{
	Menu_Begin (54, 52, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Pic (72, 32, "gfx/mp_menu.lmp");
	Menu_KeyEvent (multi_player_keyevent);
	join_menu ();
	if (do_single_player)
		Menu_Item (54, 52, "", quit_f, 0);
	Menu_Item (54, 72, "", quit_f, 0);
	Menu_Cursor (spinner);
	Menu_End ();
};



void () help_menu =
{
	Menu_Item (54, 92, "", quit_f, 0);
};

void () main_menu =
{
	Menu_Begin (0, 0, "main");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_main.lmp");
	Menu_Pic (71,32, "gfx/mainmenu.lmp");
	Menu_Cursor (spinner);
	if (do_single_player)
		single_player_menu ();
	switch (gametype ()) {
	case "netquake":
		multi_player_menu ();
		break;
	case "quakeworld":
		server_list_menu ();
		break;
	default:
		break;
	}
	options_menu ();
	help_menu ();
	Menu_Item (54, 112, "", quit_f, 0);
	Menu_End ();
};

void () menu_init =
{
	lanConfig_port_il = InputLine_Create (4, 8, ' ');
	InputLine_SetWidth (lanConfig_port_il, 10);
	lanConfig_join_il = InputLine_Create (4, 24, ' ');
	InputLine_SetWidth (lanConfig_join_il, 26);
	switch (gametype ()) {
	case "netquake":
		do_single_player = 1;
		break;
	case "quakeworld":
		do_single_player = 0;
		break;
	default:
		break;
	}
	main_menu ();
	quit_menu ();
	load_menu ();
	save_menu ();
	Menu_TopMenu ("main");
	Menu_SetQuit (quit);
};
