/*
	TexturePalette.h

	Texture Palette handling for Forge

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#import <Foundation/NSGeometry.h>
#import <Foundation/NSString.h>
#import <Foundation/NSObject.h>

#import <AppKit/NSBitmapImageRep.h>

typedef union pixel32_u {
	byte			chan[4];
	unsigned int	p;
} pixel32_t;


typedef struct texturedef_s {
	char	texture[16];
	float	rotate;
	float	shift[2];
	float	scale[2];
} texturedef_t;

typedef struct qtexture_s {
	NSString			*name;
	NSSize				size;
	NSBitmapImageRep	*rep;
	void				*data;
	pixel32_t			flatcolor;
} qtexture_t;

#define	MAX_TEXTURES	1024

extern	int				tex_count;
extern	qtexture_t 		qtextures[MAX_TEXTURES];

void	TEX_InitFromWad (NSString *path);
qtexture_t *TEX_ForName (NSString *name);

typedef struct {
	id		image;		// NSImage
	NSRect	r;
	char	*name;
	int		index;
	int		display;	// flag (on/off)
} texpal_t;

#define	TEX_INDENT	10
#define	TEX_SPACING	16

extern	id texturepalette_i;

@interface TexturePalette: NSObject
{
	char	currentwad[1024];
	id	textureList_i;
	id	textureView_i;
	id	searchField_i;
	id	sizeField_i;
	
	id	field_Xshift_i;
	id	field_Yshift_i;
	id	field_Xscale_i;
	id	field_Yscale_i;
	id	field_Rotate_i;
	
	int	viewWidth;
	int	viewHeight;
	int	selectedTexture;
}

- (NSString *) currentWad;
- initPaletteFromWadfile:(NSString *) wadFile;
- computeTextureViewSize;
- alphabetize;
- getList;
- (int) getSelectedTexture;
- setSelectedTexture: (int) which;
- (int) getSelectedTexIndex;

// Called externally
- (NSString *) selectedTextureName;
- selectTextureByName: (NSString *) name;

// New methods to replace the 2 above ones
- (texturedef_t *) textureDef;
- setTextureDef: (texturedef_t *) td;

// Action methods
- searchForTexture: (id) sender;

- clearTexinfo: (id) sender;

- incXShift: (id) sender;
- decXShift: (id) sender;

- incYShift: (id) sender;
- decYShift: (id) sender;

- incRotate: (id) sender;
- decRotate: (id) sender;

- incXScale: (id) sender;
- decXScale: (id) sender;

- incYScale: (id) sender;
- decYScale: (id) sender;

- texturedefChanged: (id) sender;
- onlyShowMapTextures: (id) sender;
- (int) searchForTextureInPalette: (NSString *) texture;
- setDisplayFlag: (int) index to: (int) value;

@end
