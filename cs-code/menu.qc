void (integer x, integer y, string text) Menu_Begin = #0;
void (void () func) Menu_Draw = #0;
void (integer x, integer y, string name) Menu_Pic = #0;
void (integer x, integer y, string name) Menu_CenterPic = #0;
void (integer x, integer y, string text, void (string text, integer key) func) Menu_Item = #0;
void (void (integer x, integer y) func) Menu_Cursor = #0;
void (integer (integer key, integer unicode, integer down) func) Menu_KeyEvent = #0;
void () Menu_End = #0;
void (string name) Menu_TopMenu = #0;
void (string name) Menu_SelectMenu = #0;
void (integer () func) Menu_SetQuit = #0;
void () Menu_Quit = #0;

void (integer x, integer y, string name) Draw_Pic = #0;
void (integer x, integer y, string name) Draw_CenterPic = #0;
void (integer x, integer y, integer chr) Draw_Character = #0;
void (integer x, integer y, string text) Draw_String = #0;
void (integer x, integer y, string text, integer n) Draw_nString = #0;
void (integer x, integer y, string text) Draw_AltString = #0;

void (string text) Cbuf_AddText = #0;
void (string text) Cbuf_InsertText = #0;
void () Cbuf_Execute = #0;
void () Cbuf_Execute_Sets = #0;

//FIXME need a proper file struct, string sucks
string (string path, string mode) File_Open = #0;
void (string file) File_Close = #0;
string (string file) File_GetLine = #0;

string (integer old, integer new, string str) String_ReplaceChar = #0;

float () random = #0;
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

// ********* LOAD

#define MAX_SAVEGAMES 12
string [MAX_SAVEGAMES] filenames;
integer [MAX_SAVEGAMES] loadable;
integer load_cursor;

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

void (string text, integer key) load_f =
{
	scan_saves ();
	Menu_SelectMenu ("load");
};

void (string text, integer key) save_f =
{
	scan_saves ();
	Menu_SelectMenu ("save");
};

void () load_draw =
{
	local integer i;

	Draw_CenterPic (160, 4, "gfx/p_load.lmp");
	for (i=0 ; i< MAX_SAVEGAMES; i++)
		Draw_String (16, 32 + 8 * i, filenames[i]);
	Draw_Character (8, 32 + load_cursor * 8, 12 + (integer (time * 4) & 1));
};

integer (integer key, integer unicode, integer down) load_keyevent =
{
};

void () load_menu =
{
	Menu_Begin (0, 0, "load");
	Menu_KeyEvent (load_keyevent);
	Menu_Draw (load_draw);
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

void (string text, integer key) quit_f =
{
	quit ();
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

void () quit_draw =
{
	text_box (56, 76, 24, 4);
	Draw_String (64, 84, quitMessage[quit_index *4 + 0]);
	Draw_String (64, 92, quitMessage[quit_index *4 + 1]);
	Draw_String (64, 100, quitMessage[quit_index *4 + 2]);
	Draw_String (64, 108, quitMessage[quit_index *4 + 3]);
};

void () quit_menu =
{
	Menu_Begin (0, 0, "quit");
	Menu_KeyEvent (quit_keyevent);
	Menu_Draw (quit_draw);
	Menu_End ();
};

void (string text, integer key) sp_start =
{
	Menu_SelectMenu (NIL);
	Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cbuf_AddText ("coop 0\n");
	Cbuf_AddText ("deathmatch 0\n");
	Cbuf_AddText ("teamplay 0\n");
	Cbuf_AddText ("map start\n");
};

void () single_player_menu =
{
	Menu_Begin (54, 32, "");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_sgl.lmp");
	Menu_Pic (72, 32, "gfx/sp_menu.lmp");
	Menu_Cursor (spinner);
	Menu_Item (54, 32, "", sp_start);
	Menu_Item (54, 52, "", load_f);
	Menu_Item (54, 72, "", save_f);
	Menu_End ();
};

void () multi_player_menu =
{
	Menu_Item (54, 52, "", quit_f);
};

void () options_menu =
{
	Menu_Item (54, 72, "", quit_f);
};

void () help_menu =
{
	Menu_Item (54, 92, "", quit_f);
};

void () main_menu =
{
	Menu_Begin (0, 0, "main");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_main.lmp");
	Menu_Pic (71,32, "gfx/mainmenu.lmp");
	Menu_Cursor (spinner);
	if (do_single_player)
		single_player_menu ();
	multi_player_menu ();
	options_menu ();
	help_menu ();
	Menu_Item (54, 112, "", quit_f);
	Menu_End ();
};

void () menu_init =
{
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
	Menu_TopMenu ("main");
	Menu_SetQuit (quit);
};
