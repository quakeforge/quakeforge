#ifndef TexturePalette_h
#define TexturePalette_h

#include <AppKit/AppKit.h>

#include "QF/qtypes.h"

typedef  union {
	byte        chan[4];
	unsigned    p;
} pixel32_t;

typedef  struct {
	char    texture[16];
	float   rotate;
	float   shift[2];
	float   scale[2];
} texturedef_t;

typedef  struct {
	char  name[16];

	int     width;
	int     height;
	NSBitmapImageRep    *rep;
	void                *data;
	pixel32_t           flatcolor;
} qtexture_t;

#define MAX_TEXTURES 1024

extern int          tex_count;
extern qtexture_t   qtextures[MAX_TEXTURES];

qtexture_t *TEX_ForName (const char *name);

typedef  struct {
	NSImageRep  *image;
	NSRect      r;
	char        *name;
	int         index;
	int         display;            // flag (on/off)
} texpal_t;

#define TEX_INDENT  10
#define TEX_SPACING 16

extern id  texturepalette_i;

@interface TexturePalette: NSObject
{
	char    currentwad[1024];
	id      textureList_i;
	id      textureView_i;
	id      searchField_i;
	id      sizeField_i;

	id  field_Xshift_i;
	id  field_Yshift_i;
	id  field_Xscale_i;
	id  field_Yscale_i;
	id  field_Rotate_i;

	int     viewWidth;
	int     viewHeight;
	int     selectedTexture;
}

- (const char *) currentWad;
- (id) initPaletteFromWadfile: (const char *)wf;
- (id) computeTextureViewSize;
- (id) alphabetize;
- (id) getList;
- (int) getSelectedTexture;
- (id) setSelectedTexture: (int)which;
- (int) getSelectedTexIndex;

// Called externally
- (const char *) getSelTextureName;
- (id) setTextureByName: (const char *)name;

// New methods to replace the 2 above ones
- (id) setTextureDef: (texturedef_t *)td;
- (id) getTextureDef: (texturedef_t *)td;

// Action methods
- (id) searchForTexture: sender;

- (id) clearTexinfo: sender;

- (id) incXShift: sender;
- (id) decXShift: sender;

- (id) incYShift: sender;
- (id) decYShift: sender;

- (id) incRotate: sender;
- (id) decRotate: sender;

- (id) incXScale: sender;
- (id) decXScale: sender;

- (id) incYScale: sender;
- (id) decYScale: sender;

- (id) texturedefChanged: sender;
- (id) onlyShowMapTextures: sender;
- (int) searchForTextureInPalette: (const char *)texture;
- (id) setDisplayFlag: (int)index
   to: (int)value;

@end
#endif // TexturePalette_h
