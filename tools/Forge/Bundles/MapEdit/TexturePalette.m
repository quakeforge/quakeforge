#include <unistd.h>

#include "QF/qendian.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/wadfile.h"
#include "QF/va.h"

#include "TexturePalette.h"
#include "Preferences.h"
#include "Map.h"
#include "Entity.h"
#include "QuakeEd.h"
#include "SetBrush.h"

#include "Storage.h"

id  texturepalette_i;

int         tex_count;
qtexture_t  qtextures[MAX_TEXTURES];

typedef  struct {
	char        name[16];
	unsigned    width, height;
	unsigned    offsets[4];         // four mip maps stored
} miptex_t;

unsigned  tex_palette[256];

unsigned  badtex_d[] = {
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,

	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0,           0,      0,      0,      0,      0,      0,      0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff
};

qtexture_t  badtex = {"notexture", 16, 16, NULL, badtex_d, {{0, 0, 255, 255}}};

void
CleanupName (const char *in, char *out)
{
	int  i;

	for (i = 0; i < 16; i++) {
		if (!in[i])
			break;
		out[i] = toupper (in[i]);
	}
	for ( ; i < 16; i++)
		out[i] = 0;
}

/*
==============
TEX_InitPalette
==============
*/
void
TEX_InitPalette (wad_t *wad, lumpinfo_t *pallump)
{
	byte    *pal, *opal;
	int     r, g, b, v;
	int     i;

	opal = pal = malloc (pallump->size);
	Qseek (wad->handle, pallump->filepos, SEEK_SET);
	Qread (wad->handle, pal, pallump->size);

	for (i = 0; i < 256; i++) {
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (r << 24) + (g << 16) + (b << 8) + 255;
		v = BigLong (v);

		tex_palette[i] = v;
	}
	free (opal);
}

/*
=================
TEX_ImageFromMiptex
=================
*/
void
TEX_ImageFromMiptex (wad_t *wad, lumpinfo_t *qtexlump)
{
	miptex_t  *qtex;
	NSBitmapImageRep    *bm;
	byte                *source;
	unsigned            *dest;
	int                 width, height, i, count;
	qtexture_t          *q;
	int                 tr, tg, tb;

	qtex = malloc (qtexlump->size);
	Qseek (wad->handle, qtexlump->filepos, SEEK_SET);
	Qread (wad->handle, qtex, qtexlump->size);

	width = LittleLong (qtex->width);
	height = LittleLong (qtex->height);

	bm = [[NSBitmapImageRep alloc]
	        initWithBitmapDataPlanes: NULL
	                      pixelsWide: width
	                      pixelsHigh: height
	                   bitsPerSample: 8
	                 samplesPerPixel: 3
	                        hasAlpha: NO
	                        isPlanar: NO
	                  colorSpaceName: NSCalibratedRGBColorSpace
	                     bytesPerRow: width * 4
	                    bitsPerPixel: 32];

	dest = (unsigned *) [bm bitmapData];
	count = width * height;
	source = (byte *) qtex + LittleLong (qtex->offsets[0]);

	q = &qtextures[tex_count];
	tex_count++;

	q->width = width;
	q->height = height;
	q->rep = bm;
	q->data = dest;

	tr = tg = tb = 0;

	for (i = 0; i < count; i++) {
		dest[i] = tex_palette[source[i]];
		tr += ((pixel32_t *) &dest[i])->chan[0];
		tg += ((pixel32_t *) &dest[i])->chan[1];
		tb += ((pixel32_t *) &dest[i])->chan[2];
	}

	q->flatcolor.chan[0] = tr / count;
	q->flatcolor.chan[1] = tg / count;
	q->flatcolor.chan[2] = tb / count;
	q->flatcolor.chan[3] = 0xff;
	free (qtex);
}

// =============================================================================

/*
=================
TEX_InitFromWad
=================
*/
int
TEX_InitFromWad (const char *path)
{
	int         i;
	const char  *newpath;
	float       start, stop;
	wad_t       *wad;
	lumpinfo_t  *lumpinfo;

	start = Sys_DoubleTime ();

	newpath = [[preferences_i getProjectPath] cString];
	newpath = va ("%s%s%s", newpath, (newpath[0] ? "/" : ""), path);

	// free any textures
	for (i = 0; i < tex_count; i++)
		[qtextures[i].rep release];

	tex_count = 0;

	Sys_Printf ("TEX_InitFromWad: loading %s\n", newpath);
	wad = wad_open (newpath);
	if (!wad) {
		NSRunAlertPanel (@"Wad Error!",
		                 [NSString stringWithFormat: @"Failed to open '%s'",
		                  newpath],
		                 @"OK", nil,
		                 nil);
		return 0;
	}

	lumpinfo = wad->lumps;

	if (strcmp (lumpinfo->name, "PALETTE")) {
		lumpinfo_t  tlump;

		Sys_Printf ("TEX_InitFromWad: %s doesn't have palette at index 0\n", path);
		lumpinfo = wad_find_lump (wad, "PALETTE");
		if (!lumpinfo)
			Sys_Error ("TEX_InitFromWad: %s doesn't have a palette", path);

		// move the palette lump to the first entry
		tlump = *lumpinfo;
		*lumpinfo = wad->lumps[0];
		wad->lumps[0] = tlump;
		lumpinfo = wad->lumps;
	}

	TEX_InitPalette (wad, lumpinfo);

	lumpinfo++;
	for (i = 1; i < wad->numlumps; i++, lumpinfo++) {
		if (lumpinfo->type != TYP_MIPTEX)
			Sys_Error ("TEX_InitFromWad: %s is not a miptex!", lumpinfo->name);
		CleanupName (lumpinfo->name, qtextures[tex_count].name);
		TEX_ImageFromMiptex (wad, lumpinfo);
	}

	wad_close (wad);

	stop = Sys_DoubleTime ();

	Sys_Printf ("loaded %s (%5.1f)\n", newpath, stop - start);
	return 1;
}

/*
=================
TEX_NumForName
=================
*/
qtexture_t *
TEX_ForName (const char *name)
{
	char        newname[16];
	int         i;
	qtexture_t  *q;

	CleanupName (name, newname);

	for (i = 0, q = qtextures; i < tex_count; i++, q++) {
		if (!strcmp (newname, q->name))
			return q;
	}

	return &badtex;
}

// ===========================================================================

@implementation TexturePalette
- (id) init
{
	[super init];
	texturepalette_i = self;
	selectedTexture = -1;
	return self;
}

- (void) display
{
	[[textureView_i superview] display];
}

- (const char *) currentWad
{
	return currentwad;
}

- (id) initPaletteFromWadfile: (const char *)wf
{
	int         i;
	texpal_t    t;
	qtexture_t  *q;

	strcpy (currentwad, wf);
	[map_i makeGlobalPerform: @selector (flushTextures)];
	selectedTexture = -1;

	// Init textures WAD
	if (!TEX_InitFromWad (wf))
		return self;

	// Create STORAGE
	if (textureList_i) {
		[textureList_i empty];
	} else {
		textureList_i = [[Storage alloc]   initCount: 0
		                                 elementSize: sizeof (texpal_t)
		                                 description: NULL];
	}
	// Init STORAGE

	for (i = 0, q = qtextures; i < tex_count; i++, q++) {
		t.image = q->rep;
		t.r.size.width = [t.image pixelsWide];
		if (t.r.size.width < 64)
			t.r.size.width = 64;
		t.r.size.height = [t.image pixelsHigh] + TEX_SPACING;
		t.name = q->name;
		t.index = i;
		t.display = 1;
		[textureList_i addElement: &t];
	}

	// Calculate size of TextureView
	[self alphabetize];
	[self computeTextureViewSize];
	[textureView_i setParent: self];
	[self setSelectedTexture: 0];

	return self;
}

//  Return texture STORAGE list
- (id) getList
{
	return textureList_i;
}

//  Alphabetize texture list - reverse order!
- (id) alphabetize
{
	int         i;
	int         max;
	texpal_t    *t1p;
	texpal_t    *t2p;
	texpal_t    t1;
	texpal_t    t2;
	int         found;

	max = [textureList_i count];
	found = 1;
	while (found) {
		found = 0;
		for (i = 0; i < max - 1; i++) {
			t1p = [textureList_i elementAt: i];
			t2p = [textureList_i elementAt: i + 1];
			if (strcmp (t1p->name, t2p->name) < 0) {
				t1 = *t1p;
				t2 = *t2p;
				[textureList_i replaceElementAt: i with: &t2];
				[textureList_i replaceElementAt: i + 1 with: &t1];
				found = 1;
			}
		}
	}
	return self;
}

- (id) computeTextureViewSize
{
	int         i;
	int         max;
	int         x;
	texpal_t    *t;
	int         y;
	id          view;
	NSRect      b;
	int         maxwidth;
	int         maxheight;
	NSPoint     pt;

	max = [textureList_i count];
	y = 0;
	maxheight = 0;
	x = TEX_INDENT;

	view = [textureView_i superview];
	b = [view bounds];
	maxwidth = b.size.width;

	for (i = 0; i < max; i++) {
		t = [textureList_i elementAt: i];
		if (x + t->r.size.width + TEX_INDENT > maxwidth) {
			x = TEX_INDENT;
			y += maxheight;
			maxheight = 0;
		}
		if (t->r.size.height > maxheight)
			maxheight = t->r.size.height;
		t->r.origin.x = x;
		t->r.origin.y = y;
		x += t->r.size.width + TEX_INDENT;
		if (i == max - 1)
			y += t->r.size.height;
	}

	viewWidth = maxwidth;
	viewHeight = y + TEX_SPACING;
	[textureView_i setFrameSize: NSMakeSize (viewWidth, viewHeight)];
	pt.x = pt.y = 0;
	[textureView_i scrollPoint: pt];

	return self;
}

- (id) windowResized
{
	[self computeTextureViewSize];
	return self;
}

- (id) texturedefChanged: sender
{
	if ([map_i numSelected]) {
		if ([[map_i currentEntity] modifiable]) {
			[map_i makeSelectedPerform: @selector
			 (takeCurrentTexture)];
			[quakeed_i updateAll];
		} else {
			Sys_Printf ("can't modify spawned entities\n");
		}
	}
	[quakeed_i makeFirstResponder: quakeed_i];
	return self;
}

- (id) clearTexinfo: sender
{
	[field_Xshift_i setFloatValue: 0];
	[field_Yshift_i setFloatValue: 0];
	[field_Xscale_i setFloatValue: 1];
	[field_Yscale_i setFloatValue: 1];
	[field_Rotate_i setFloatValue: 0];

	[self texturedefChanged: self];

	return self;
}

//
//  Set the selected texture
//
- (id) setSelectedTexture: (int)which
{
	texpal_t    *t;
	NSRect      r;

// wipe the fields
	[self clearTexinfo: self];

	if (which != selectedTexture) {
		[textureView_i deselect];
		selectedTexture = which;
		t = [textureList_i elementAt: which];
		r = t->r;
		r.size.width += TEX_INDENT * 2;
		r.size.height += TEX_INDENT * 2;
		r.origin.x -= TEX_INDENT;
		r.origin.y -= TEX_INDENT;
		[textureView_i scrollRectToVisible: r];
		[textureView_i display];
		[sizeField_i setStringValue:
		 [NSString stringWithFormat: @"%d x %d",
		  (int) t->r.size.width,
		  (int) t->r.size.height - TEX_SPACING]];
	}

	[self texturedefChanged: self];

	return self;
}

//
//  Return the selected texture index
//
- (int) getSelectedTexture
{
	return selectedTexture;
}

//
//  Return the original tex_ index of the selected texture
//  so the texture info can be indexed from tex_images, etc.
//
- (int) getSelectedTexIndex
{
	texpal_t  *t;

	if (selectedTexture == -1)
		return -1;
	t = [textureList_i elementAt: selectedTexture];
	return t->index;
}

//
//  Return the name of the selected texture
//
- (const char *) getSelTextureName
{
	texpal_t  *t;

	if (selectedTexture == -1)
		return NULL;
	t = [textureList_i elementAt: selectedTexture];
	return t->name;
}

//
//  Set selected texture by texture name
//
- (id) setTextureByName: (const char *)name
{
	texpal_t    *t;
	int         i;
	int         max;
	char        *nm = strdup (name);

	max = [textureList_i count];
	CleanupName (nm, nm);
	for (i = 0; i < max; i++) {
		t = [textureList_i elementAt: i];
		if (!strcmp (t->name, nm)) {
			free (nm);
			[self setSelectedTexture: i];
			return self;
		}
	}
	free (nm);
	return self;
}

// ===================================================
//
//  Action methods
//
// ===================================================

//
//  Search for texture named in searchField
//
- (id) searchForTexture: sender
{
	int     i;
	int     max;
	int     len;
	NSMutableString     *strname;
	const char          *name;
	texpal_t            *t;

	if (selectedTexture == -1)
		return self;

	max = [textureList_i count];
	strname = [[sender stringValue] mutableCopy];
	[strname uppercaseString];
	[sender setStringValue: strname];
	name = [strname cString];
	len = strlen (name);

	for (i = selectedTexture - 1; i >= 0; i--) {
		t = [textureList_i elementAt: i];
		if (!strncmp (t->name, name, len)) {
			[self setTextureByName: t->name];
			[sender selectText: sender];
			[self texturedefChanged: self];
			return self;
		}
	}

	for (i = max - 1; i >= selectedTexture; i--) {
		t = [textureList_i elementAt: i];
		if (!strncmp (t->name, name, len)) {
			[self setTextureByName: t->name];
			[sender selectText: sender];
			[self texturedefChanged: self];
			return self;
		}
	}

	[self texturedefChanged: self];
	return self;
}

//
//  Set texture def from outside TexturePalette
//
- (id) setTextureDef: (texturedef_t *)td
{
	[self setTextureByName: td->texture];

	[field_Xshift_i setFloatValue: td->shift[0]];
	[field_Yshift_i setFloatValue: td->shift[1]];
	[field_Xscale_i setFloatValue: td->scale[0]];
	[field_Yscale_i setFloatValue: td->scale[1]];
	[field_Rotate_i setFloatValue: td->rotate];

	[self texturedefChanged: self];

	return self;
}

//
//  Return the current texture def to passed *
//
- (id) getTextureDef: (texturedef_t *)td
{
	if (selectedTexture == -1) {
		memset (td, 0, sizeof (*td));
		strcpy (td->texture, "notexture");
		return self;
	}

	strncpy (td->texture, [self getSelTextureName], 16);

	td->shift[0] = [field_Xshift_i floatValue];
	td->shift[1] = [field_Yshift_i floatValue];
	td->scale[0] = [field_Xscale_i floatValue];
	td->scale[1] = [field_Yscale_i floatValue];
	td->rotate = [field_Rotate_i floatValue];

	return self;
}

// ============================================================================

//
//  Change value in a field
//
- (id) changeField: (id)
   field by: (int)amount
{
	int  val;

	val = [field intValue];
	val += amount;
	[field setIntValue: val];

	[self texturedefChanged: self];

	return self;
}

//
//  Inc/Dec the XShift field
//
- (id) incXShift: sender
{
	[self changeField: field_Xshift_i by: 8];
	return self;
}

- (id) decXShift: sender
{
	[self changeField: field_Xshift_i by: -8];
	return self;
}

//
//  Inc/Dec the YShift field
//
- (id) incYShift: sender
{
	[self changeField: field_Yshift_i by: 8];
	return self;
}

- (id) decYShift: sender
{
	[self changeField: field_Yshift_i by: -8];
	return self;
}

//
//  Inc/Dec the Rotate field
//
- (id) incRotate: sender
{
	[self changeField: field_Rotate_i by: 90];
	return self;
}

- (id) decRotate: sender
{
	[self changeField: field_Rotate_i by: -90];
	return self;
}

//
//  Inc/Dec the Xscale field
//
- (id) incXScale: sender
{
	[field_Xscale_i setIntValue: 1];
	[self texturedefChanged: self];
	return self;
}

- (id) decXScale: sender
{
	[field_Xscale_i setIntValue: -1];
	[self texturedefChanged: self];
	return self;
}

//
//  Inc/Dec the Yscale field
//
- (id) incYScale: sender
{
	[field_Yscale_i setIntValue: 1];
	[self texturedefChanged: self];
	return self;
}

- (id) decYScale: sender
{
	[field_Yscale_i setIntValue: -1];
	[self texturedefChanged: self];
	return self;
}

// ============================================================================

//
//  Search for texture in entire palette
//  Return index of texturedef, or -1 if unsuccessful
//
- (int) searchForTextureInPalette: (const char *)texture
{
	int         i;
	int         max;
	texpal_t    *t;

	if (selectedTexture == -1)
		return -1;

	max = [textureList_i count];

	for (i = 0; i < max; i++) {
		t = [textureList_i elementAt: i];
		if (!strcmp (t->name, texture))
			return i;
	}
	return -1;
};

//
// Scan thru map & display only textures that are in map
//
- (id) onlyShowMapTextures: sender
{
	int         max;
	int         i;
	int         j;
	id          brushes;
	SetBrush    *b;
	int         numfaces;
	face_t      *f;
	int         index;

	// Turn 'em off
	if ([sender intValue]) {
		max = [textureList_i count];
		for (i = 0; i < max; i++)
			[self setDisplayFlag: i to: 0];

		brushes = [map_i objectAtIndex: 0];
		max = [brushes count];
		for (i = 0; i < max; i++) {
			b = (SetBrush *) [brushes objectAtIndex: i];
			numfaces = [b getNumBrushFaces];
			for (j = 0; j < numfaces; j++) {
				f = [b getBrushFace: j];
				index = [self searchForTextureInPalette: f->texture.
				         texture];
				if (index >= 0)
					[self setDisplayFlag: index to: 1];
			}
		}
	}
	// Turn 'em on
	else {
		max = [textureList_i count];
		for (i = 0; i < max; i++)
			[self setDisplayFlag: i to: 1];
	}

	[textureView_i display];

	return self;
}

- (id) setDisplayFlag: (int)
   index to: (int)value
{
	texpal_t  *tp;

	tp = [textureList_i elementAt: index];
	tp->display = value;
	return self;
};

@end
