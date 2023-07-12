/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

//
// spritegen.c: generates a .spr file from a series of .lbm frame files.
// Result is stored in /raid/quake/id1/sprites/<scriptname>.spr.
//

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakeio.h"
#include "QF/script.h"
#include "QF/spritegn.h"
#include "QF/sys.h"

#define MAX_BUFFER_SIZE		0x100000
#define MAX_FRAMES			1000

tex_t          *image;
dsprite_t		sprite;
byte			*lumpbuffer, *plump;
dstring_t		*spritedir;
dstring_t		*spriteoutname;
int				framesmaxs[2];
int				framecount;

script_t scr;

typedef struct {
	spriteframetype_t	type;		// single frame or group of frames
	void				*pdata;		// either a dspriteframe_t or group info
	float				interval;	// only used for frames in groups
	int					numgroupframes;	// only used by group headers
} spritepackage_t;

spritepackage_t	frames[MAX_FRAMES];

static void FinishSprite (void);
static void Cmd_Spritename (void);


/*
============
WriteFrame
============
*/
static void
WriteFrame (QFile *spriteouthandle, int framenum)
{
	dspriteframe_t	*pframe;
	dspriteframe_t	frametemp;

	pframe = (dspriteframe_t *)frames[framenum].pdata;
	frametemp.origin[0] = LittleLong (pframe->origin[0]);
	frametemp.origin[1] = LittleLong (pframe->origin[1]);
	frametemp.width = LittleLong (pframe->width);
	frametemp.height = LittleLong (pframe->height);

	Qwrite (spriteouthandle, &frametemp, sizeof (frametemp));
	Qwrite (spriteouthandle,
			   (byte *)(pframe + 1),
			   pframe->height * pframe->width);
}


/*
============
WriteSprite
============
*/
static void
WriteSprite (QFile *spriteouthandle)
{
	int			i, groupframe, curframe;
	dsprite_t	spritetemp;

	sprite.boundingradius = sqrt (((framesmaxs[0] >> 1) *
								   (framesmaxs[0] >> 1)) +
								  ((framesmaxs[1] >> 1) *
								   (framesmaxs[1] >> 1)));

//
// write out the sprite header
//
	spritetemp.type = LittleLong (sprite.type);
	spritetemp.boundingradius = LittleFloat (sprite.boundingradius);
	spritetemp.width = LittleLong (framesmaxs[0]);
	spritetemp.height = LittleLong (framesmaxs[1]);
	spritetemp.numframes = LittleLong (sprite.numframes);
	spritetemp.beamlength = LittleFloat (sprite.beamlength);
	spritetemp.synctype = LittleFloat (sprite.synctype);
	spritetemp.version = LittleLong (SPR_VERSION);
	spritetemp.ident = LittleLong (IDHEADER_SPR);

	Qwrite (spriteouthandle, &spritetemp, sizeof(spritetemp));

//
// write out the frames
//
	curframe = 0;

	for (i=0 ; i<sprite.numframes ; i++)
	{
		Qwrite (spriteouthandle, &frames[curframe].type,
				   sizeof(frames[curframe].type));

		if (frames[curframe].type == SPR_SINGLE)
		{
		//
		// single (non-grouped) frame
		//
			WriteFrame (spriteouthandle, curframe);
			curframe++;
		}
		else
		{
			int					j, numframes;
			dspritegroup_t		dsgroup;
			float				totinterval;

			groupframe = curframe;
			curframe++;
			numframes = frames[groupframe].numgroupframes;

		//
		// set and write the group header
		//
			dsgroup.numframes = LittleLong (numframes);

			Qwrite (spriteouthandle, &dsgroup, sizeof(dsgroup));

		//
		// write the interval array
		//
			totinterval = 0.0;

			for (j=0 ; j<numframes ; j++)
			{
				dspriteinterval_t	temp;

				totinterval += frames[groupframe+1+j].interval;
				temp.interval = LittleFloat (totinterval);

				Qwrite (spriteouthandle, &temp, sizeof(temp));
			}

			for (j=0 ; j<numframes ; j++)
			{
				WriteFrame (spriteouthandle, curframe);
				curframe++;
			}
		}
	}
}

/*
==============
LoadScreen
==============
*/
static void
LoadScreen (const char *name)
{
	QFile      *file;
	printf ("grabbing from %s...\n",name);
	file = Qopen (name, "rb");
	if (!file)
		Sys_Error ("could not open");
	image = LoadPCX (file, false, 0, 1);
}


/*
===============
Cmd_Type
===============
*/
static void
Cmd_Type (void)
{
	Script_GetToken (&scr, false);
	if (!strcmp (Script_Token (&scr), "vp_parallel_upright"))
		sprite.type = SPR_VP_PARALLEL_UPRIGHT;
	else if (!strcmp (Script_Token (&scr), "facing_upright"))
		sprite.type = SPR_FACING_UPRIGHT;
	else if (!strcmp (Script_Token (&scr), "vp_parallel"))
		sprite.type = SPR_VP_PARALLEL;
	else if (!strcmp (Script_Token (&scr), "oriented"))
		sprite.type = SPR_ORIENTED;
	else if (!strcmp (Script_Token (&scr), "vp_parallel_oriented"))
		sprite.type = SPR_VP_PARALLEL_ORIENTED;
	else
		Sys_Error ("Bad sprite type\n");
}


/*
===============
Cmd_Beamlength
===============
*/
static void
Cmd_Beamlength (void)
{
	Script_GetToken (&scr, false);
	sprite.beamlength = atof (Script_Token (&scr));
}


/*
===============
Cmd_Load
===============
*/
static void
Cmd_Load (void)
{
	Script_GetToken (&scr, false);
	LoadScreen (Script_Token (&scr));
}


/*
===============
Cmd_Frame
===============
*/
static void
Cmd_Frame (void)
{
	int             x,y,xl,yl,xh,yh,w,h;
	byte            *screen_p;
	int             linedelta;
	dspriteframe_t	*pframe;
	int				pix;

	Script_GetToken (&scr, false);
	xl = atoi (Script_Token (&scr));
	Script_GetToken (&scr, false);
	yl = atoi (Script_Token (&scr));
	Script_GetToken (&scr, false);
	w = atoi (Script_Token (&scr));
	Script_GetToken (&scr, false);
	h = atoi (Script_Token (&scr));

	if ((xl & 0x07) || (yl & 0x07) || (w & 0x07) || (h & 0x07))
		Sys_Error ("Sprite dimensions not multiples of 8\n");

	if ((w > 255) || (h > 255))
		Sys_Error ("Sprite has a dimension longer than 255");

	xh = xl+w;
	yh = yl+h;

	pframe = (dspriteframe_t *)plump;
	frames[framecount].pdata = pframe;
	frames[framecount].type = SPR_SINGLE;

	if (Script_TokenAvailable (&scr, false))
	{
		Script_GetToken (&scr, false);
		frames[framecount].interval = atof (Script_Token (&scr));
		if (frames[framecount].interval <= 0.0)
			Sys_Error ("Non-positive interval");
	}
	else
	{
		frames[framecount].interval = 0.1;
	}

	if (Script_TokenAvailable (&scr, false))
	{
		Script_GetToken (&scr, false);
		pframe->origin[0] = -atoi (Script_Token (&scr));
		Script_GetToken (&scr, false);
		pframe->origin[1] = atoi (Script_Token (&scr));
	}
	else
	{
		pframe->origin[0] = -(w >> 1);
		pframe->origin[1] = h >> 1;
	}

	pframe->width = w;
	pframe->height = h;

	if (w > framesmaxs[0])
		framesmaxs[0] = w;

	if (h > framesmaxs[1])
		framesmaxs[1] = h;

	plump = (byte *)(pframe + 1);

	screen_p = image->data + yl* image->width + xl;
	linedelta =  image->width - w;

	for (y=yl ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
		{
			pix = *screen_p;
			*screen_p++ = 0;
//			if (pix == 255)
//				pix = 0;
			*plump++ = pix;
		}
		screen_p += linedelta;
	}

	framecount++;
	if (framecount >= MAX_FRAMES)
		Sys_Error ("Too many frames; increase MAX_FRAMES\n");
}


/*
===============
Cmd_GroupStart
===============
*/
static void
Cmd_GroupStart (void)
{
	int			groupframe;

	groupframe = framecount++;

	frames[groupframe].type = SPR_GROUP;
	frames[groupframe].numgroupframes = 0;

	while (1)
	{
		if (!Script_GetToken (&scr, true))
			Sys_Error ("End of file during group");

		if (!strcmp (Script_Token (&scr), "$frame"))
		{
			Cmd_Frame ();
			frames[groupframe].numgroupframes++;
		}
		else if (!strcmp (Script_Token (&scr), "$load"))
		{
			Cmd_Load ();
		}
		else if (!strcmp (Script_Token (&scr), "$groupend"))
		{
			break;
		}
		else
		{
			Sys_Error ("$frame, $load, or $groupend expected\n");
		}

	}

	if (frames[groupframe].numgroupframes == 0)
		Sys_Error ("Empty group\n");
}


/*
===============
ParseScript
===============
*/
static void
ParseScript (void)
{
	while (1)
	{
		if (!Script_GetToken (&scr, true))
			break;

		if (!strcmp (Script_Token (&scr), "$load"))
		{
			Cmd_Load ();
		}
		if (!strcmp (Script_Token (&scr), "$spritename"))
		{
			Cmd_Spritename ();
		}
		else if (!strcmp (Script_Token (&scr), "$type"))
		{
			Cmd_Type ();
		}
		else if (!strcmp (Script_Token (&scr), "$beamlength"))
		{
			Cmd_Beamlength ();
		}
		else if (!strcmp (Script_Token (&scr), "$sync"))
		{
			sprite.synctype = ST_SYNC;
		}
		else if (!strcmp (Script_Token (&scr), "$frame"))
		{
			Cmd_Frame ();
			sprite.numframes++;
		}
		else if (!strcmp (Script_Token (&scr), "$load"))
		{
			Cmd_Load ();
		}
		else if (!strcmp (Script_Token (&scr), "$groupstart"))
		{
			Cmd_GroupStart ();
			sprite.numframes++;
		}
	}
}

/*
==============
Cmd_Spritename
==============
*/
static void
Cmd_Spritename (void)
{
	if (sprite.numframes)
		FinishSprite ();

	Script_GetToken (&scr, false);
	dsprintf (spriteoutname, "%s%s.spr", spritedir->str, Script_Token (&scr));
	memset (&sprite, 0, sizeof(sprite));
	framecount = 0;

	framesmaxs[0] = -9999999;
	framesmaxs[1] = -9999999;

	lumpbuffer = malloc (MAX_BUFFER_SIZE * 2);	// *2 for padding
	if (!lumpbuffer)
		Sys_Error ("Couldn't get buffer memory");

	plump = lumpbuffer;
	sprite.synctype = ST_RAND;	// default
}

/*
==============
FinishSprite
==============
*/
static void
FinishSprite (void)
{
	QFile	*spriteouthandle;

	if (sprite.numframes == 0)
		Sys_Error ("no frames\n");

	if (!spriteoutname->str)
		Sys_Error ("Didn't name sprite file");

	if ((plump - lumpbuffer) > MAX_BUFFER_SIZE)
		Sys_Error ("Sprite package too big; increase MAX_BUFFER_SIZE");

	spriteouthandle = Qopen (spriteoutname->str, "wb");
	printf ("saving in %s\n", spriteoutname->str);
	WriteSprite (spriteouthandle);
	Qclose (spriteouthandle);

	printf ("spritegen: successful\n");
	printf ("%d frame(s)\n", sprite.numframes);
	printf ("%d ungrouped frame(s), including group headers\n", framecount);

	dstring_clearstr (spriteoutname);		// clear for a new sprite
}

/*
==============
main

==============
*/
int main (int argc, char **argv)
{
	int         i, bytes;
	QFile      *file;
	char       *buf;

	if (argc != 2)
		Sys_Error ("usage: spritegen file.qc");

	spritedir = dstring_newstr ();
	spriteoutname = dstring_newstr ();

	i = 1;

	//SetQdirFromPath (argv[i]);
	//ExtractFilePath (argv[i], spritedir);	// chop the filename

//
// load the script
//
	file = Qopen (argv[i], "rt");
	if (!file)
		Sys_Error ("couldn't open %s. %s", argv[i], strerror(errno));
	bytes = Qfilesize (file);
	buf = malloc (bytes + 1);
	bytes = Qread (file, buf, bytes);
	buf[bytes] = 0;
	Qclose (file);
	Script_Start (&scr, argv[i], buf);

	ParseScript ();
	FinishSprite ();

	return 0;
}
