
typedef union pixel32_u {
	byte		chan[4];
	unsigned	p;
} pixel32_t;


typedef struct texturedef_s {
	char	texture[16];
	float	rotate;
	float	shift[2];
	float	scale[2];
} texturedef_t;

typedef struct qtexture_s {
	char		name[16];

	int			width;
	int			height;
	NSBitmapImageRep	*rep;
	void		*data;
	pixel32_t	flatcolor;
} qtexture_t;

#define	MAX_TEXTURES	1024

extern	int					tex_count;
extern	qtexture_t 		qtextures[MAX_TEXTURES];

void	TEX_InitFromWad (char *path);
qtexture_t *TEX_ForName (char *name);

typedef struct {
	id		image;		// NXImage
	NSRect	r;
	char	*name;
	int		index;
	int		display;	// flag (on/off)
} texpal_t;

#define	TEX_INDENT	10
#define	TEX_SPACING	16

extern	id texturepalette_i;

@interface TexturePalette:Object
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
