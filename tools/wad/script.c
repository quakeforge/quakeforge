/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: 2004/9/28

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <errno.h>

#include "QF/GL/defines.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/png.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/wad.h"
#include "QF/va.h"

#include "wad.h"

static dstring_t destfile = {&dstring_default_mem};
static bool savesingle = false;
static wad_t *wadfile;
dstring_t  *lumpname;
tex_t      *image;
byte       *lumpbuffer, *lump_p;

typedef struct {
	const char *name;
	void      (*func)(script_t *script);
} command_t;

static void
unimp (script_t *script)
{
	Sys_Error ("unimplemented: %d", script->line);
}

command_t   commands[] = {
	{"palette",   GrabPalette},
	{"colormap",  unimp},//GrabColormap},
	{"qpic",      unimp},//GrabPic},
	{"--",        unimp},
	{"miptex",    GrabMip},
	{"raw",       unimp},//GrabRaw},

	{"colormap2", unimp},//GrabColormap2},

	{0, 0},
};

static void
load_image (const char *name)
{
	QFile      *file;
	tex_t      *tex;
	int         i, pixels;
	byte       *s, *d;

	if (!(file = Qopen (name, "rb")))
		Sys_Error ("couldn't open %s. %s", name, strerror(errno));
	if (!(tex = LoadPNG (file, 1)))
		Sys_Error ("couldn't read %s as PNG", name);

	pixels = tex->width * tex->height;
	image = malloc (pixels * 4 + sizeof (tex_t));
	image->width = tex->width;
	image->height = tex->height;
	image->format = tex_rgba;
	image->palette = 0;
	switch (tex->format) {
		case tex_palette:
			for (i = 0, s = tex->data, d = image->data; i < pixels; i++) {
				const byte *v = tex->palette + *s++ * 3;
				*d++ = *v++;
				*d++ = *v++;
				*d++ = *v++;
				*d++ = 255;
			}
			break;
		case tex_l:
			for (i = 0, s = tex->data, d = image->data; i < pixels; i++) {
				byte        l = *s++;
				*d++ = l;
				*d++ = l;
				*d++ = l;
				*d++ = 255;
			}
			break;
		case tex_a:
			for (i = 0, s = tex->data, d = image->data; i < pixels; i++) {
				*d++ = 255;
				*d++ = 255;
				*d++ = 255;
				*d++ = *s++;
			}
			break;
		case tex_la:
			for (i = 0, s = tex->data, d = image->data; i < pixels; i++) {
				byte        l = *s++;
				*d++ = l;
				*d++ = l;
				*d++ = l;
				*d++ = *s++;
			}
			break;
		case tex_rgb:
			for (i = 0, s = tex->data, d = image->data; i < pixels; i++) {
				*d++ = *s++;
				*d++ = *s++;
				*d++ = *s++;
				*d++ = 255;
			}
			break;
		case tex_rgba:
			memcpy (image->data, tex->data, pixels * 4);
			break;
		default:
			Sys_Error ("unknown texture type");
	}
}

static void
write_file (void)
{
	QFile      *file;
	const char *name;

	name = va (0, "%s/%s.lmp", destfile.str, lumpname->str);
	if (!(file = Qopen (name, "wb")))
		Sys_Error ("couldn't open %s. %s", name, strerror(errno));
	Qwrite (file, lumpbuffer, lump_p - lumpbuffer);
	Qclose (file);
	free (lumpbuffer);
	lumpbuffer = lump_p = 0;
}

static void
write_lump (int type)
{
	if (!wadfile) {
		wadfile = wad_create (destfile.str);
		if (!wadfile)
			Sys_Error ("couldn't create %s. %s", destfile.str,
					   strerror(errno));
	}
	wad_add_data (wadfile, lumpname->str, type, lumpbuffer,
				  lump_p - lumpbuffer);
	free (lumpbuffer);
	lumpbuffer = lump_p = 0;
}

static void
parse_script (script_t *script)
{
	int         cmd;

	while (Script_GetToken (script, true)) {
		if (strcasecmp ("$LOAD", script->token->str) == 0) {
			Script_GetToken (script, false);
			load_image (script->token->str);
			continue;
		}
		if (strcasecmp ("$DEST", script->token->str) == 0) {
			Script_GetToken (script, false);
			dstring_copystr (&destfile, script->token->str);
			continue;
		}
		if (strcasecmp ("$SINGLEDEST", script->token->str) == 0) {
			Script_GetToken (script, false);
			dstring_copystr (&destfile, script->token->str);
			savesingle = true;
			continue;
		}

		if (!lumpname)
			lumpname = dstring_newstr ();
		dstring_copystr (lumpname, script->token->str);

		Script_GetToken (script, false);
		for (cmd = 0; commands[cmd].name; cmd++) {
			if (!strcasecmp (script->token->str, commands[cmd].name)) {
				commands[cmd].func (script);
				break;
			}
		}
		if (!commands[cmd].name)
			Sys_Error ("Unrecognized token '%s' at line %i", script->token->str,
					   script->line);
		//grabbed++;

		if (savesingle)
			write_file ();
		else
			write_lump (TYP_LUMPY + cmd);
	}
}

void
process_wad_script (const char *name)
{
	char       *buf;
	QFile      *file;
	int         bytes;
	script_t   *script;

	file = Qopen (name, "rt");
	if (!file)
		Sys_Error ("couldn't open %s. %s", name, strerror(errno));
	bytes = Qfilesize (file);
	buf = malloc (bytes + 1);
	bytes = Qread (file, buf, bytes);
	buf[bytes] = 0;
	Qclose (file);

	dstring_copystr (&destfile, name);
	dstring_appendstr (&destfile, ".wad");

	script = Script_New ();
	Script_Start (script, name, buf);

	parse_script (script);

	if (wadfile)
		wad_close (wadfile);
}
