/*
	keys.h

	Key definitions and prototypes

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/


#ifndef _KEYS_H
#define _KEYS_H

#ifndef __QFCC__
# include "QF/qtypes.h"
# include "QF/quakeio.h"
#endif

// these are the key numbers that should be passed to Key_Event

typedef enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	QFK_UNKNOWN		= 0,
	QFK_FIRST			= 0,
	QFK_BACKSPACE		= 8,
	QFK_TAB			= 9,
	QFK_CLEAR			= 12,
	QFK_RETURN		= 13,
	QFK_PAUSE			= 19,
	QFK_ESCAPE		= 27,
	QFK_SPACE			= 32,
	QFK_EXCLAIM		= 33,
	QFK_QUOTEDBL		= 34,
	QFK_HASH			= 35,
	QFK_DOLLAR		= 36,
	QFK_PERCENT		= 37,
	QFK_AMPERSAND		= 38,
	QFK_QUOTE			= 39,
	QFK_LEFTPAREN		= 40,
	QFK_RIGHTPAREN	= 41,
	QFK_ASTERISK		= 42,
	QFK_PLUS			= 43,
	QFK_COMMA			= 44,
	QFK_MINUS			= 45,
	QFK_PERIOD		= 46,
	QFK_SLASH			= 47,
	QFK_0				= 48,
	QFK_1				= 49,
	QFK_2				= 50,
	QFK_3				= 51,
	QFK_4				= 52,
	QFK_5				= 53,
	QFK_6				= 54,
	QFK_7				= 55,
	QFK_8				= 56,
	QFK_9				= 57,
	QFK_COLON			= 58,
	QFK_SEMICOLON		= 59,
	QFK_LESS			= 60,
	QFK_EQUALS		= 61,
	QFK_GREATER		= 62,
	QFK_QUESTION		= 63,
	QFK_AT			= 64,
	/* 
	   Skip uppercase letters
	 */
	QFK_LEFTBRACKET	= 91,
	QFK_BACKSLASH		= 92,
	QFK_RIGHTBRACKET	= 93,
	QFK_CARET			= 94,
	QFK_UNDERSCORE	= 95,
	QFK_BACKQUOTE		= 96,
	QFK_a				= 97,
	QFK_b				= 98,
	QFK_c				= 99,
	QFK_d				= 100,
	QFK_e				= 101,
	QFK_f				= 102,
	QFK_g				= 103,
	QFK_h				= 104,
	QFK_i				= 105,
	QFK_j				= 106,
	QFK_k				= 107,
	QFK_l				= 108,
	QFK_m				= 109,
	QFK_n				= 110,
	QFK_o				= 111,
	QFK_p				= 112,
	QFK_q				= 113,
	QFK_r				= 114,
	QFK_s				= 115,
	QFK_t				= 116,
	QFK_u				= 117,
	QFK_v				= 118,
	QFK_w				= 119,
	QFK_x				= 120,
	QFK_y				= 121,
	QFK_z				= 122,
	QFK_BRACELEFT		= 123,
	QFK_BAR				= 124,
	QFK_BRACERIGHT		= 125,
	QFK_ASCIITILDE		= 126,
	QFK_DELETE			= 127,
	/* End of ASCII mapped keysyms */

	/* International keyboard syms */
	QFK_WORLD_0		= 160,		/* 0xA0 */
	QFK_WORLD_1		= 161,
	QFK_WORLD_2		= 162,
	QFK_WORLD_3		= 163,
	QFK_WORLD_4		= 164,
	QFK_WORLD_5		= 165,
	QFK_WORLD_6		= 166,
	QFK_WORLD_7		= 167,
	QFK_WORLD_8		= 168,
	QFK_WORLD_9		= 169,
	QFK_WORLD_10		= 170,
	QFK_WORLD_11		= 171,
	QFK_WORLD_12		= 172,
	QFK_WORLD_13		= 173,
	QFK_WORLD_14		= 174,
	QFK_WORLD_15		= 175,
	QFK_WORLD_16		= 176,
	QFK_WORLD_17		= 177,
	QFK_WORLD_18		= 178,
	QFK_WORLD_19		= 179,
	QFK_WORLD_20		= 180,
	QFK_WORLD_21		= 181,
	QFK_WORLD_22		= 182,
	QFK_WORLD_23		= 183,
	QFK_WORLD_24		= 184,
	QFK_WORLD_25		= 185,
	QFK_WORLD_26		= 186,
	QFK_WORLD_27		= 187,
	QFK_WORLD_28		= 188,
	QFK_WORLD_29		= 189,
	QFK_WORLD_30		= 190,
	QFK_WORLD_31		= 191,
	QFK_WORLD_32		= 192,
	QFK_WORLD_33		= 193,
	QFK_WORLD_34		= 194,
	QFK_WORLD_35		= 195,
	QFK_WORLD_36		= 196,
	QFK_WORLD_37		= 197,
	QFK_WORLD_38		= 198,
	QFK_WORLD_39		= 199,
	QFK_WORLD_40		= 200,
	QFK_WORLD_41		= 201,
	QFK_WORLD_42		= 202,
	QFK_WORLD_43		= 203,
	QFK_WORLD_44		= 204,
	QFK_WORLD_45		= 205,
	QFK_WORLD_46		= 206,
	QFK_WORLD_47		= 207,
	QFK_WORLD_48		= 208,
	QFK_WORLD_49		= 209,
	QFK_WORLD_50		= 210,
	QFK_WORLD_51		= 211,
	QFK_WORLD_52		= 212,
	QFK_WORLD_53		= 213,
	QFK_WORLD_54		= 214,
	QFK_WORLD_55		= 215,
	QFK_WORLD_56		= 216,
	QFK_WORLD_57		= 217,
	QFK_WORLD_58		= 218,
	QFK_WORLD_59		= 219,
	QFK_WORLD_60		= 220,
	QFK_WORLD_61		= 221,
	QFK_WORLD_62		= 222,
	QFK_WORLD_63		= 223,
	QFK_WORLD_64		= 224,
	QFK_WORLD_65		= 225,
	QFK_WORLD_66		= 226,
	QFK_WORLD_67		= 227,
	QFK_WORLD_68		= 228,
	QFK_WORLD_69		= 229,
	QFK_WORLD_70		= 230,
	QFK_WORLD_71		= 231,
	QFK_WORLD_72		= 232,
	QFK_WORLD_73		= 233,
	QFK_WORLD_74		= 234,
	QFK_WORLD_75		= 235,
	QFK_WORLD_76		= 236,
	QFK_WORLD_77		= 237,
	QFK_WORLD_78		= 238,
	QFK_WORLD_79		= 239,
	QFK_WORLD_80		= 240,
	QFK_WORLD_81		= 241,
	QFK_WORLD_82		= 242,
	QFK_WORLD_83		= 243,
	QFK_WORLD_84		= 244,
	QFK_WORLD_85		= 245,
	QFK_WORLD_86		= 246,
	QFK_WORLD_87		= 247,
	QFK_WORLD_88		= 248,
	QFK_WORLD_89		= 249,
	QFK_WORLD_90		= 250,
	QFK_WORLD_91		= 251,
	QFK_WORLD_92		= 252,
	QFK_WORLD_93		= 253,
	QFK_WORLD_94		= 254,
	QFK_WORLD_95		= 255,		/* 0xFF */

	/* Numeric keypad */
	QFK_KP0			= 256,
	QFK_KP1			= 257,
	QFK_KP2			= 258,
	QFK_KP3			= 259,
	QFK_KP4			= 260,
	QFK_KP5			= 261,
	QFK_KP6			= 262,
	QFK_KP7			= 263,
	QFK_KP8			= 264,
	QFK_KP9			= 265,
	QFK_KP_PERIOD		= 266,
	QFK_KP_DIVIDE		= 267,
	QFK_KP_MULTIPLY	= 268,
	QFK_KP_MINUS		= 269,
	QFK_KP_PLUS		= 270,
	QFK_KP_ENTER		= 271,
	QFK_KP_EQUALS		= 272,

	/* Arrows + Home/End pad */
	QFK_UP			= 273,
	QFK_DOWN			= 274,
	QFK_RIGHT			= 275,
	QFK_LEFT			= 276,
	QFK_INSERT		= 277,
	QFK_HOME			= 278,
	QFK_END			= 279,
	QFK_PAGEUP		= 280,
	QFK_PAGEDOWN		= 281,

	/* Function keys */
	QFK_F1			= 282,
	QFK_F2			= 283,
	QFK_F3			= 284,
	QFK_F4			= 285,
	QFK_F5			= 286,
	QFK_F6			= 287,
	QFK_F7			= 288,
	QFK_F8			= 289,
	QFK_F9			= 290,
	QFK_F10			= 291,
	QFK_F11			= 292,
	QFK_F12			= 293,
	QFK_F13			= 294,
	QFK_F14			= 295,
	QFK_F15			= 296,

	/* Key state modifier keys */
	QFK_NUMLOCK		= 300,
	QFK_CAPSLOCK		= 301,
	QFK_SCROLLOCK		= 302,
	QFK_RSHIFT		= 303,
	QFK_LSHIFT		= 304,
	QFK_RCTRL			= 305,
	QFK_LCTRL			= 306,
	QFK_RALT			= 307,
	QFK_LALT			= 308,
	QFK_RMETA			= 309,
	QFK_LMETA			= 310,
	QFK_LSUPER		= 311,		/* Left "Windows" key */
	QFK_RSUPER		= 312,		/* Right "Windows" key */
	QFK_MODE			= 313,		/* "Alt Gr" key */
	QFK_COMPOSE		= 314,		/* Multi-key compose key */

	/* Miscellaneous function keys */
	QFK_HELP			= 315,
	QFK_PRINT			= 316,
	QFK_SYSREQ		= 317,
	QFK_BREAK			= 318,
	QFK_MENU			= 319,
	QFK_POWER			= 320,		/* Power Macintosh power key */
	QFK_EURO			= 321,		/* Some european keyboards */
	QFK_UNDO			= 322,

	/* Japanese keys */
	QFK_KANJI,						/* Kanji, Kanji convert */
	QFK_MUHENKAN,					/* Cancel Conversion */
	QFK_HENKAN,						/* Alias for Henkan_Mode */
	QFK_ROMAJI,						/* to Romaji */
	QFK_HIRAGANA,					/* to Hiragana */
	QFK_KATAKANA,					/* to Katakana */
	QFK_HIRAGANA_kATAKANA,			/* Hiragana/Katakana toggle */
	QFK_ZENKAKU,					/* to Zenkaku */
	QFK_HANKAKU,					/* to Hankaku */
	QFK_ZENKAKU_HANKAKU,			/* Zenkaku/Hankaku toggle */
	QFK_TOUROKU,					/* Add to Dictionary */
	QFK_MASSYO,						/* Delete from Dictionary */
	QFK_KANA_LOCK,					/* Kana Lock */
	QFK_KANA_SHIFT,					/* Kana Shift */
	QFK_EISU_SHIFT,					/* Alphanumeric Shift */
	QFK_EISU_TOGGLE,				/* Alphanumeric toggle */
	QFK_KANJI_BANGOU,				/* Codeinput */
	QFK_ZEN_KOHO,					/* Multiple/All Candidate(s) */
	QFK_MAE_KOHO,					/* Previous Candidate */

	/* Add any other keys here */

//
// mouse buttons generate virtual keys
//
	QFM_BUTTON1,
	QFM_BUTTON2,
	QFM_BUTTON3,
	QFM_WHEEL_UP,
	QFM_WHEEL_DOWN,
	QFM_BUTTON6,
	QFM_BUTTON7,

//
// joystick buttons
//
	QFJ_BUTTON1,
	QFJ_BUTTON2,
	QFJ_BUTTON3,
	QFJ_BUTTON4,
	QFJ_BUTTON5,
	QFJ_BUTTON6,
	QFJ_BUTTON7,
	QFJ_BUTTON8,
	QFJ_BUTTON9,
	QFJ_BUTTON10,
	QFJ_BUTTON11,
	QFJ_BUTTON12,
	QFJ_BUTTON13,
	QFJ_BUTTON14,
	QFJ_BUTTON15,
	QFJ_BUTTON16,
	QFJ_BUTTON17,
	QFJ_BUTTON18,
	QFJ_BUTTON19,
	QFJ_BUTTON20,
	QFJ_BUTTON21,
	QFJ_BUTTON22,
	QFJ_BUTTON23,
	QFJ_BUTTON24,
	QFJ_BUTTON25,
	QFJ_BUTTON26,
	QFJ_BUTTON27,
	QFJ_BUTTON28,
	QFJ_BUTTON29,
	QFJ_BUTTON30,
	QFJ_BUTTON31,
	QFJ_BUTTON32,

	QFK_LAST
} knum_t;

typedef enum {
	IMT_CONSOLE,
	IMT_0,
	IMT_1,
	IMT_2,
	IMT_3,
	IMT_4,
	IMT_5,
	IMT_6,
	IMT_7,
	IMT_8,
	IMT_9,
	IMT_10,
	IMT_11,
	IMT_12,
	IMT_13,
	IMT_14,
	IMT_15,
	IMT_16,
	IMT_LAST,
} imt_t;											// Input Mapping Table

// key_none should, preferably, be last
typedef enum {
	key_game,
	key_console,
	key_message,
	key_menu,
	key_none
} keydest_t;

#ifndef __QFCC__
typedef struct {
	int     down[2];        // key nums holding it down
	int     state;          // low bit is down state
} kbutton_t;

extern keydest_t	key_dest;
extern imt_t		game_target;
extern knum_t       key_togglemenu;
extern knum_t       key_toggleconsole;

extern struct keybind_s {
	char *str;
} keybindings[IMT_LAST][QFK_LAST];
extern int		keydown[QFK_LAST];

struct cbuf_s;
void Key_Event (knum_t key, short unicode, qboolean down);
void Key_Init (struct cbuf_s *cb);
void Key_Init_Cvars (void);
void Key_WriteBindings (QFile *f);
void Key_ClearStates (void);
const char *Key_GetBinding (imt_t imt, knum_t key);
void Key_SetBinding (imt_t target, knum_t keynum, const char *binding);


const char *Key_KeynumToString (knum_t keynum);
struct progs_s;
void Key_Progs_Init (struct progs_s *pr);
#endif

#endif // _KEYS_H
