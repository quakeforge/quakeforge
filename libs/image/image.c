/*
	image.c

	General image handling

	Copyright (C) 2003 Harry Roberts

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";
	
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "compat.h"

#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/texture.h"
#include "QF/png.h"
#include "QF/tga.h"
#include "QF/pcx.h"
#include "QF/image.h"

tex_t *
LoadImage (char *imageFile, QFile *fp)
{
	int tmp;
	char *tmpFile, *ext;
	tex_t *tex = NULL;
	
	/* Get the file name without extension */
	tmp = strlen (imageFile);
	tmpFile = strdup (imageFile);
	ext = strrchr (tmpFile, '.');
	ext[0] = '\0';
	
	if (strlen(tmpFile) != (tmp - 4))
		return (NULL); /* Extension must be 3 characters long */
	
	tmp = 0;
	
	/* Check for a .png */
	strcat (ext, ".png");
	QFS_FOpenFile (tmpFile, &fp);
	if (fp) {
		tex = LoadPNG (fp);
		Qclose (fp);
		tmp = 1;
	}
	
	/* Check for a .tga */
	if (tmp == 0) {
		ext = strrchr (tmpFile, '.');
		ext[0] = '\0';
		strcat (ext, ".tga");
		QFS_FOpenFile (tmpFile, &fp);
		
		if (fp) {
			tex = LoadTGA (fp);
			Qclose (fp);
			tmp = 1;
		}
	}
	
	/* Check for a .pcx */
	/*if (tmp == 0) {
		ext = strrchr (tmpFile, '.');
		ext[0] = '\0';
		strcat (ext, ".tga");
		QFS_FOpenFile (tmpFile, &fp);
		
		if (fp) {
			tex = LoadPCX (fp); // FIXME: needs extra arguments, how should we be passed them?
			Qclose (fp);
			tmp = 1;
		}
	}*/
	
	free (tmpFile);
	return (tex);
}
