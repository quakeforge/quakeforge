/*
	glsl_draw.c

	2D drawing support for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/ui/font.h"
#include "QF/ui/view.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_draw.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

typedef struct pic_data_s {
	subpic_t   *subpic;
} picdata_t;

typedef struct cachepic_s {
	struct cachepic_s *next;
	char       *name;
	qpic_t     *pic;
} cachepic_t;

typedef struct {
	float       xyst[4];
	float       color[4];
} drawvert_t;

typedef struct glslfont_s {
	font_t     *font;
	GLuint      texid;
} glslfont_t;

typedef struct glfontset_s
    DARRAY_TYPE (glslfont_t) glslfontset_t;

typedef struct glslbatch_s {
	int         shader;
	GLenum      mode;
	GLuint      texid;
	uint32_t    start;
	uint32_t    count;
} glslbatch_t;

typedef struct glbatchset_s
    DARRAY_TYPE (glslbatch_t) glslbatchset_t;

static glslfontset_t glsl_fonts = DARRAY_STATIC_INIT (16);

static glslbatchset_t glsl_batches = DARRAY_STATIC_INIT (16);
static dstring_t *glsl_queue;

static const char *twod_vert_effects[] =
{
	"QuakeForge.Vertex.2d",
	0
};

static const char *twod_frag_effects[] =
{
	"QuakeForge.Fragment.palette",
	"QuakeForge.Fragment.2d",
	0
};

static const char *twod_alpha_frag_effects[] =
{
	"QuakeForge.Fragment.2d.alpha",
	0
};

static float proj_matrix[16];

static struct {
	int         program;
	shaderparam_t texture;
	shaderparam_t palette;
	shaderparam_t matrix;
	shaderparam_t vertex;
	shaderparam_t color;
} quake_2d = {
	.texture = {"texture", 1},
	.palette = {"palette", 1},
	.matrix = {"mvp_mat", 1},
	.vertex = {"vertex", 0},
	.color = {"vcolor", 0},
};

static struct {
	int         program;
	shaderparam_t texture;
	shaderparam_t matrix;
	shaderparam_t vertex;
	shaderparam_t color;
} alpha_2d = {
	.texture = {"texture", 1},
	.matrix = {"mvp_mat", 1},
	.vertex = {"vertex", 0},
	.color = {"vcolor", 0},
};

static scrap_t *draw_scrap;		// hold all 2d images
static GLuint scrap_texid;
static byte white_block[8 * 8];
static qpic_t *conchars;
static int  conback_texture;
static qpic_t *crosshair_pic;
static qpic_t *white_pic;
static qpic_t *backtile_pic;
static hashtab_t *pic_cache;
static int glsl_conback_texnum;
static int glsl_2d_scale = 1;
static cvar_t glsl_conback_texnum_cvar = {
	.name = "glsl_conback_texnum",
	.description =
		"bind conback to this texture for debugging",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &glsl_conback_texnum },
};

static qpic_t *
make_glpic (const char *name, qpic_t *p)
{
	qpic_t     *pic = 0;

	if (p) {
		pic = malloc (offsetof (qpic_t, data[sizeof (picdata_t)]));
		pic->width = p->width;
		pic->height = p->height;
		__auto_type pd = (picdata_t *) pic->data;
		pd->subpic = GLSL_ScrapSubpic (draw_scrap, pic->width, pic->height);
		GLSL_SubpicUpdate (pd->subpic, p->data, 1);
	}
	return pic;
}

static void
pic_free (qpic_t *pic)
{
	if (!pic) {
		return;
	}

	__auto_type pd = (picdata_t *) pic->data;

	GLSL_SubpicDelete (pd->subpic);
	free (pic);
}

//FIXME nicer allocator
static cachepic_t *
new_cachepic (const char *name, qpic_t *pic)
{
	cachepic_t *cp;

	cp = malloc (sizeof (cachepic_t));
	cp->name = strdup (name);
	cp->pic = pic;
	return cp;
}

static const char *
cachepic_getkey (const void *_cp, void *unused)
{
	return ((cachepic_t *) _cp)->name;
}

static void
cachepic_free (void *_cp, void *unused)
{
	cachepic_t *cp = (cachepic_t *) _cp;
	pic_free (cp->pic);
	free (cp->name);
	free (cp);
}

static qpic_t *
pic_data (const char *name, int w, int h, const byte *data)
{
	qpic_t     *pic;
	qpic_t     *glpic;

	pic = malloc (offsetof (qpic_t, data[w * h]));
	pic->width = w;
	pic->height = h;
	memcpy (pic->data, data, pic->width * pic->height);
	glpic = make_glpic (name, pic);
	free (pic);
	return glpic;
}

static void
queue_verts (int shader, GLenum mode, GLuint texid, uint32_t count)
{
	auto batch = &glsl_batches.a[glsl_batches.size - 1];
	if (!glsl_batches.size
		|| batch->shader != shader
		|| batch->mode != mode
		|| batch->texid != texid) {
		DARRAY_APPEND (&glsl_batches, ((glslbatch_t) {
			.shader = shader,
			.mode = mode,
			.texid = texid,
			.start = glsl_queue->size,
		}));
		batch = &glsl_batches.a[glsl_batches.size - 1];
	}
	batch->count += count;
}

static void
make_quad (qpic_t *pic, float x, float y, int w, int h,
		   int srcx, int srcy, int srcw, int srch, drawvert_t verts[6],
		   const float *color)
{
	__auto_type pd = (picdata_t *) pic->data;
	subpic_t   *sp = pd->subpic;
	float       sl, sh, tl, th;

	srcx += sp->rect->x;
	srcy += sp->rect->y;
	sl = (srcx) * sp->size;
	sh = sl + (srcw) * sp->size;
	tl = (srcy) * sp->size;
	th = tl + (srch) * sp->size;

	verts[0].xyst[0] = x;
	verts[0].xyst[1] = y;
	verts[0].xyst[2] = sl;
	verts[0].xyst[3] = tl;

	verts[1].xyst[0] = x + w;
	verts[1].xyst[1] = y;
	verts[1].xyst[2] = sh;
	verts[1].xyst[3] = tl;

	verts[2].xyst[0] = x + w;
	verts[2].xyst[1] = y + h;
	verts[2].xyst[2] = sh;
	verts[2].xyst[3] = th;

	verts[3].xyst[0] = x;
	verts[3].xyst[1] = y;
	verts[3].xyst[2] = sl;
	verts[3].xyst[3] = tl;

	verts[4].xyst[0] = x + w;
	verts[4].xyst[1] = y + h;
	verts[4].xyst[2] = sh;
	verts[4].xyst[3] = th;

	verts[5].xyst[0] = x;
	verts[5].xyst[1] = y + h;
	verts[5].xyst[2] = sl;
	verts[5].xyst[3] = th;

	QuatCopy (color, verts[0].color);
	QuatCopy (color, verts[1].color);
	QuatCopy (color, verts[2].color);
	QuatCopy (color, verts[3].color);
	QuatCopy (color, verts[4].color);
	QuatCopy (color, verts[5].color);
}

static void
draw_pic (float x, float y, int w, int h, qpic_t *pic,
		  int srcx, int srcy, int srcw, int srch,
		  const float *color)
{
	drawvert_t  verts[6];
	void       *v;
	int         size = sizeof (verts);

	queue_verts (quake_2d.program, GL_TRIANGLES, scrap_texid, 6);
	make_quad (pic, x, y, w, h, srcx, srcy, srcw, srch, verts, color);
	glsl_queue->size += size;
	dstring_adjust (glsl_queue);
	v = glsl_queue->str + glsl_queue->size - size;
	memcpy (v, verts, size);
}

qpic_t *
glsl_Draw_MakePic (int width, int height, const byte *data)
{
	return pic_data (0, width, height, data);
}

void
glsl_Draw_DestroyPic (qpic_t *pic)
{
	pic_free (pic);
}

qpic_t *
glsl_Draw_PicFromWad (const char *name)
{
	return make_glpic (name, W_GetLumpName (name));
}

qpic_t *
glsl_Draw_CachePic (const char *path, bool alpha)
{
	qpic_t     *p, *pic;
	cachepic_t *cpic;

	if ((cpic = Hash_Find (pic_cache, path)))
		return cpic->pic;
	if (strlen (path) < 4 || strcmp (path + strlen (path) - 4, ".lmp")
		|| !(p = (qpic_t *) QFS_LoadFile (QFS_FOpenFile (path), 0))) {
		return nullptr;
	}

	pic = make_glpic (path, p);
	free (p);
	cpic = new_cachepic (path, pic);
	Hash_Add (pic_cache, cpic);
	return pic;
}

void
glsl_Draw_UncachePic (const char *path)
{
	Hash_Free (pic_cache, Hash_Del (pic_cache, path));
}

void
glsl_Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
	static quat_t color = { 1, 1, 1, 0 };
	qpic_t     *p;
	int         cx, cy, n;
#define draw(px,py,pp) \
	draw_pic (px, py, pp->width, pp->height, pp, \
			  0, 0, pp->width, pp->height, color)

	color[3] = alpha / 255.0;
	// draw left side
	cx = x;
	cy = y;
	p = glsl_Draw_CachePic ("gfx/box_tl.lmp", true);
	draw (cx, cy, p);
	p = glsl_Draw_CachePic ("gfx/box_ml.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		draw (cx, cy, p);
	}
	p = glsl_Draw_CachePic ("gfx/box_bl.lmp", true);
	draw (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = glsl_Draw_CachePic ("gfx/box_tm.lmp", true);
		draw (cx, cy, p);
		p = glsl_Draw_CachePic ("gfx/box_mm.lmp", true);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = glsl_Draw_CachePic ("gfx/box_mm2.lmp", true);
			draw (cx, cy, p);
		}
		p = glsl_Draw_CachePic ("gfx/box_bm.lmp", true);
		draw (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = glsl_Draw_CachePic ("gfx/box_tr.lmp", true);
	draw (cx, cy, p);
	p = glsl_Draw_CachePic ("gfx/box_mr.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		draw (cx, cy, p);
	}
	p = glsl_Draw_CachePic ("gfx/box_br.lmp", true);
	draw (cx, cy + 8, p);
#undef draw
}

static void
Draw_ClearCache (int phase, void *data)
{
	if (phase)
		return;
	Hash_FlushTable (pic_cache);
}

void
glsl_Draw_Init (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         i;
	int         frag, vert;
	qpic_t     *pic;

	pic_cache = Hash_NewTable (127, cachepic_getkey, cachepic_free, 0, 0);
	QFS_GamedirCallback (Draw_ClearCache, 0);
	//FIXME temporary work around for the timing of cvar creation and palette
	//loading
	//crosshaircolor->callback (crosshaircolor);

	glsl_queue = dstring_new ();

	vert_shader = GLSL_BuildShader (twod_vert_effects);
	frag_shader = GLSL_BuildShader (twod_frag_effects);
	vert = GLSL_CompileShader ("quakeico.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quake2d.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_2d.program = GLSL_LinkProgram ("quake2d", vert, frag);
	GLSL_ResolveShaderParam (quake_2d.program, &quake_2d.texture);
	GLSL_ResolveShaderParam (quake_2d.program, &quake_2d.palette);
	GLSL_ResolveShaderParam (quake_2d.program, &quake_2d.matrix);
	GLSL_ResolveShaderParam (quake_2d.program, &quake_2d.vertex);
	GLSL_ResolveShaderParam (quake_2d.program, &quake_2d.color);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

	vert_shader = GLSL_BuildShader (twod_vert_effects);
	frag_shader = GLSL_BuildShader (twod_alpha_frag_effects);
	vert = GLSL_CompileShader ("quakeico.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("alpha2d.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	alpha_2d.program = GLSL_LinkProgram ("alpha2d", vert, frag);
	GLSL_ResolveShaderParam (alpha_2d.program, &alpha_2d.texture);
	GLSL_ResolveShaderParam (alpha_2d.program, &alpha_2d.matrix);
	GLSL_ResolveShaderParam (alpha_2d.program, &alpha_2d.vertex);
	GLSL_ResolveShaderParam (alpha_2d.program, &alpha_2d.color);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

	draw_scrap = GLSL_CreateScrap (2048, GL_LUMINANCE, 0);
	scrap_texid = GLSL_ScrapTexture (draw_scrap);

	draw_chars = W_GetLumpName ("conchars");
	if (draw_chars) {
		for (i = 0; i < 256 * 64; i++) {
			if (draw_chars[i] == 0) {
				draw_chars[i] = 255;		// proper transparent color
			}
		}
		conchars = pic_data ("conchars", 128, 128, draw_chars);
	} else {
		qpic_t     *charspic = Draw_Font8x8Pic ();
		conchars = pic_data ("conchars", charspic->width,
							 charspic->height, charspic->data);
		free (charspic);
	}

	pic = (qpic_t *) QFS_LoadFile (QFS_FOpenFile ("gfx/conback.lmp"), 0);
	if (pic) {
		SwapPic (pic);
		conback_texture = GLSL_LoadQuakeTexture ("conback",
											   pic->width, pic->height,
											   pic->data);
		free (pic);
	}

	pic = Draw_CrosshairPic ();
	crosshair_pic = make_glpic ("crosshair", pic);
	free (pic);

	memset (white_block, 0xfe, sizeof (white_block));
	white_pic = pic_data ("white_block", 8, 8, white_block);

	backtile_pic = glsl_Draw_PicFromWad ("backtile");
	if (!backtile_pic) {
		backtile_pic = white_pic;
	}

	Cvar_Register (&glsl_conback_texnum_cvar, 0, 0);
}

void
glsl_Draw_Shutdown (void)
{
	pic_free (conchars);
	pic_free (crosshair_pic);
	if (backtile_pic != white_pic) {
		pic_free (backtile_pic);
	}
	pic_free (white_pic);

	dstring_delete (glsl_queue);

	Hash_DelTable (pic_cache);

	GLSL_DestroyScrap (draw_scrap);
	DARRAY_CLEAR (&glsl_fonts);
	DARRAY_CLEAR (&glsl_batches);
}

static inline void
queue_character (int x, int y, byte chr)
{
	quat_t      color = {1, 1, 1, 1};
	int         cx, cy;

	cx = chr % 16;
	cy = chr / 16;
	draw_pic (x, y, 8, 8, conchars, cx * 8, cy * 8, 8, 8, color);
}

static void
bind_quake_2d (GLuint texid)
{
	qfeglUseProgram (quake_2d.program);
	qfeglEnableVertexAttribArray (quake_2d.vertex.location);
	qfeglEnableVertexAttribArray (quake_2d.color.location);

	qfeglUniformMatrix4fv (quake_2d.matrix.location, 1, false, proj_matrix);

	qfeglUniform1i (quake_2d.palette.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	qfeglUniform1i (quake_2d.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, texid);

	qfeglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void
bind_alpha_2d (GLuint texid)
{
	qfeglUseProgram (alpha_2d.program);
	qfeglEnableVertexAttribArray (alpha_2d.vertex.location);
	qfeglEnableVertexAttribArray (alpha_2d.color.location);

	qfeglUniformMatrix4fv (alpha_2d.matrix.location, 1, false, proj_matrix);

	qfeglBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);

	qfeglUniform1i (alpha_2d.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, texid);
}

static void
flush_2d (void)
{
	GLSL_ScrapFlush (draw_scrap);

	int shader = -1;
	GLuint texid = 0;
	GLuint vertex_location = 0;
	GLuint color_location = 0;

	for (size_t i = 0; i < glsl_batches.size; i++) {
		auto batch = &glsl_batches.a[i];
		if (shader != batch->shader) {
			if (batch->shader == quake_2d.program) {
				bind_quake_2d (batch->texid);
				vertex_location = quake_2d.vertex.location;
				color_location = quake_2d.color.location;
			} else {
				bind_alpha_2d (batch->texid);
				vertex_location = alpha_2d.vertex.location;
				color_location = alpha_2d.color.location;
			}
			shader = batch->shader;
			texid = batch->texid;
		} else if (texid != batch->texid) {
			qfeglBindTexture (GL_TEXTURE_2D, batch->texid);
			texid = batch->texid;
		}

		auto vdata = glsl_queue->str + batch->start;
		auto cdata = glsl_queue->str + batch->start + 16;
		qfeglVertexAttribPointer (vertex_location, 4, GL_FLOAT, 0, 32, vdata);
		qfeglVertexAttribPointer (color_location, 4, GL_FLOAT, 0, 32, cdata);

		qfeglDrawArrays (batch->mode, 0, batch->count);
	}
	glsl_queue->size = 0;
	DARRAY_RESIZE (&glsl_batches, 0);

	qfeglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void
glsl_Draw_CharBuffer (int x, int y, draw_charbuffer_t *buffer)
{
	const byte *line = (byte *) buffer->chars;
	int         width = buffer->width;
	int         height = buffer->height;
	while (height-- > 0) {
		for (int i = 0; i < width; i++) {
			glsl_Draw_Character (x + i * 8, y, line[i]);
		}
		line += width;
		y += 8;
	}
}

void
glsl_Draw_Character (int x, int y, unsigned int chr)
{
	chr &= 255;

	if (chr == 32)
		return;							// space
	if (y <= -8)
		return;							// totally off screen

	queue_character (x, y, chr);
}

void
glsl_Draw_String (int x, int y, const char *str)
{
	byte        chr;

	if (!str || !str[0])
		return;							// totally off screen
	if (y <= -8)
		return;							// totally off screen

	while (*str) {
		if ((chr = *str++) != 32)
			queue_character (x, y, chr);
		x += 8;
	}
}

void
glsl_Draw_nString (int x, int y, const char *str, int count)
{
	byte        chr;

	if (!str || !str[0])
		return;							// totally off screen
	if (y <= -8)
		return;							// totally off screen

	while (count-- && *str) {
		if ((chr = *str++) != 32)
			queue_character (x, y, chr);
		x += 8;
	}
}

void
glsl_Draw_AltString (int x, int y, const char *str)
{
	byte        chr;

	if (!str || !str[0])
		return;
	if (y <= -8)
		return;							// totally off screen

	while (*str) {
		if ((chr = *str++ | 0x80) != (0x80 | 32)) {		// Don't render spaces
			queue_character (x, y, chr);
		}
		x += 8;
	}
}

static void
draw_crosshair_plus (int ch, int x, int y)
{
	glsl_Draw_Character (x - 4, y - 4, '+');
}

static void
draw_crosshair_pic (int ch, int x, int y)
{
	static const int pos[CROSSHAIR_COUNT][4] = {
		{0,               0,                CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
		{CROSSHAIR_WIDTH, 0,                CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
		{0,               CROSSHAIR_HEIGHT, CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
		{CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT, CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
	};
	const int *p = pos[ch - 1];

	draw_pic (x - CROSSHAIR_WIDTH / 2 + 1, y - CROSSHAIR_HEIGHT / 2 + 1,
			  CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT, crosshair_pic,
			  p[0], p[1], p[2], p[3], crosshair_color);
}

static void (*crosshair_func[]) (int ch, int x, int y) = {
	draw_crosshair_plus,
	draw_crosshair_pic,
	draw_crosshair_pic,
	draw_crosshair_pic,
	draw_crosshair_pic,
};

void
glsl_Draw_CrosshairAt (int ch, int x, int y)
{
	unsigned    c = ch - 1;

	if (c >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	crosshair_func[c] (c, x, y);
}

void
glsl_Draw_Crosshair (void)
{
	int         x, y;
	int         s = 2 * glsl_2d_scale;

	x = vid.width / s + cl_crossx;
	y = vid.height / s + cl_crossy;

	glsl_Draw_CrosshairAt (crosshair, x, y);
}

void
glsl_Draw_Pic (int x, int y, qpic_t *pic)
{
	static quat_t color = { 1, 1, 1, 1};
	draw_pic (x, y, pic->width, pic->height, pic,
			  0, 0, pic->width, pic->height, color);
}

void
glsl_Draw_FitPic (int x, int y, int width, int height, qpic_t *pic)
{
	static quat_t color = { 1, 1, 1, 1};
	draw_pic (x, y, width, height, pic, 0, 0, pic->width, pic->height, color);
}

void
glsl_Draw_Picf (float x, float y, qpic_t *pic)
{
	static quat_t color = { 1, 1, 1, 1};
	draw_pic (x, y, pic->width, pic->height, pic,
			  0, 0, pic->width, pic->height, color);
}

void
glsl_Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
				  int height)
{
	static quat_t color = { 1, 1, 1, 1};
	draw_pic (x, y, width, height, pic, srcx, srcy, width, height, color);
}

void
glsl_Draw_ConsoleBackground (int lines, byte alpha)
{
	int         s = glsl_2d_scale;
	float       ofs = (vid.height - s * lines) / (float) vid.height;
	quat_t      color = {1, 1, 1, bound (0, alpha, 255) / 255.0};
	drawvert_t  verts[] = {
		{{            0,     0, 0, ofs}},
		{{vid.width / s,     0, 1, ofs}},
		{{vid.width / s, lines, 1,   1}},
		{{            0,     0, 0, ofs}},
		{{vid.width / s, lines, 1,   1}},
		{{            0, lines, 0,   1}},
	};

	GLSL_FlushText (); // Flush text that should be rendered before the console

	QuatCopy (color, verts[0].color);
	QuatCopy (color, verts[1].color);
	QuatCopy (color, verts[2].color);
	QuatCopy (color, verts[3].color);
	QuatCopy (color, verts[4].color);
	QuatCopy (color, verts[5].color);

	if (glsl_conback_texnum)
		qfeglBindTexture (GL_TEXTURE_2D, glsl_conback_texnum);
	else
		qfeglBindTexture (GL_TEXTURE_2D, conback_texture);

	qfeglVertexAttribPointer (quake_2d.vertex.location, 4, GL_FLOAT,
							 0, sizeof (drawvert_t), &verts[0].xyst);
	qfeglVertexAttribPointer (quake_2d.color.location, 4, GL_FLOAT,
							 0, sizeof (drawvert_t), &verts[0].color);

	qfeglDrawArrays (GL_TRIANGLES, 0, 6);
}

void
glsl_Draw_TileClear (int x, int y, int w, int h)
{
	static quat_t color = { 1, 1, 1, 1 };
	vrect_t    *tile_rect = VRect_New (x, y, w, h);
	vrect_t    *sub = VRect_New (0, 0, 0, 0);	// filled in later;
	__auto_type pd = (picdata_t *) backtile_pic->data;
	subpic_t   *sp = pd->subpic;
	int         sub_sx, sub_sy, sub_ex, sub_ey;
	int         i, j;

	sub_sx = x / sp->width;
	sub_sy = y / sp->height;
	sub_ex = (x + w + sp->width - 1) / sp->width;
	sub_ey = (y + h + sp->height - 1) / sp->height;
	for (j = sub_sy; j < sub_ey; j++) {
		for (i = sub_sx; i < sub_ex; i++) {
			vrect_t    *t = sub;

			sub->x = i * sp->width;
			sub->y = j * sp->height;
			sub->width = sp->width;
			sub->height = sp->height;
			sub = VRect_Intersect (sub, tile_rect);
			VRect_Delete (t);
			draw_pic (sub->x, sub->y, sub->width, sub->height, backtile_pic,
					  sub->x % sp->width, sub->y % sp->height,
					  sub->width, sub->height, color);
		}
	}
	VRect_Delete (sub);
	VRect_Delete (tile_rect);
	flush_2d ();
}

void
glsl_Draw_Fill (int x, int y, int w, int h, int c)
{
	quat_t      color;

	VectorScale (vid.palette + c * 3, 1.0f/255.0f, color);
	color[3] = 1.0;
	draw_pic (x, y, w, h, white_pic, 0, 0, 8, 8, color);
}

void
glsl_Draw_FillRGBA (int x, int y, int w, int h, const quat_t rgba)
{
	draw_pic (x, y, w, h, white_pic, 0, 0, 8, 8, rgba);
}

void
glsl_Draw_Line (int x0, int y0, int x1, int y1, int c)
{
	__auto_type pd = (picdata_t *) white_pic->data;
	subpic_t   *sp = pd->subpic;
	float       sl = sp->rect->x * sp->size;
	float       sh = sp->rect->x * sp->size;
	float       tl = sp->rect->y * sp->size;
	float       th = sp->rect->y * sp->size;

	quat_t      color = { VectorExpand (vid.palette + c * 3), 255 };
	QuatScale (color, 1/255.0, color);
	drawvert_t  verts[2] = {
		{ .xyst = { x0, y0, sl, tl }, .color = { QuatExpand (color) }, },
		{ .xyst = { x1, y1, sh, th }, .color = { QuatExpand (color) }, },
	};

	void       *v;
	int         size = sizeof (verts);

	queue_verts (quake_2d.program, GL_LINES, scrap_texid, 2);
	glsl_queue->size += size;
	dstring_adjust (glsl_queue);
	v = glsl_queue->str + glsl_queue->size - size;
	memcpy (v, verts, size);
}

void
glsl_LineGraph (int x, int y, int *h_vals, int count, int height)
{
	static int colors[] = { 0xd0, 0x4f, 0x6f };

	while (count-- > 0) {
		int         h = *h_vals++;
		int         c = h < 9998 || h > 10000 ? 0xfe : colors[h - 9998];
		h = min (h, height);
		glsl_Draw_Line (x, y, x, y - h, c);
		x++;
	}
}

static inline void
draw_blendscreen (quat_t color)
{
	draw_pic (0, 0, vid.width, vid.height, white_pic, 0, 0, 8, 8, color);
}

void
glsl_Draw_FadeScreen (void)
{
	static quat_t color = { 0, 0, 0, 0.7 };

	GLSL_FlushText (); // Flush text that should be rendered before the menu
	draw_blendscreen (color);
}

static void
ortho_mat (float *proj, float xmin, float xmax, float ymin, float ymax,
		   float znear, float zfar)
{
	proj[0] = 2 / (xmax - xmin);
	proj[4] = 0;
	proj[8] = 0;
	proj[12] = -(xmax + xmin) / (xmax - xmin);

	proj[1] = 0;
	proj[5] = 2 / (ymax - ymin);
	proj[9] = 0;
	proj[13] = -(ymax + ymin) / (ymax - ymin);

	proj[2] = 0;
	proj[6] = 0;
	proj[10] = -2 / (zfar - znear);
	proj[14] = -(zfar + znear) / (zfar - znear);

	proj[3] = 0;
	proj[7] = 0;
	proj[11] = 0;
	proj[15] = 1;
}

static void
set_2d (int width, int height)
{
	qfeglViewport (0, 0, vid.width, vid.height);

	qfeglDisable (GL_DEPTH_TEST);
	qfeglDisable (GL_CULL_FACE);

	ortho_mat (proj_matrix, 0, width, height, 0, -99999, 99999);
}

void
GLSL_Set2D (void)
{
    set_2d (vid.width, vid.height);
}

void
GLSL_Set2DScaled (void)
{
	set_2d (vid.width / glsl_2d_scale, vid.height / glsl_2d_scale);
}

void
glsl_Draw_SetScale (int scale)
{
	glsl_2d_scale = scale;
}

void
GLSL_End2D (void)
{
	qfeglDisableVertexAttribArray (quake_2d.vertex.location);
	qfeglDisableVertexAttribArray (quake_2d.color.location);
}

void
GLSL_DrawReset (void)
{
	glsl_queue->size = 0;
	DARRAY_RESIZE (&glsl_batches, 0);
}

void
GLSL_FlushText (void)
{
	if (glsl_batches.size)
		flush_2d ();
}

void
glsl_Draw_BlendScreen (quat_t color)
{
	if (!color[3])
		return;
	draw_blendscreen (color);
}

int
glsl_Draw_AddFont (font_t *rfont)
{
	int         fontid = glsl_fonts.size;
	DARRAY_OPEN_AT (&glsl_fonts, fontid, 1);
	glslfont_t *font = &glsl_fonts.a[fontid];

	font->font = rfont;
	tex_t       tex = {
		.width = rfont->scrap.width,
		.height = rfont->scrap.height,
		.format = tex_a,
		.loaded = true,
		.data = rfont->scrap_bitmap,
	};
	font->texid = GLSL_LoadTex ("", 1, &tex);
	return fontid;
}
#if 0
typedef struct {
	vrect_t    *glyph_rects;
	dstring_t  *batch;
	int         width;
	int         height;
	float       color[4];
} glslrgctx_t;

static void
glsl_render_glyph (uint32_t glyphid, int x, int y, void *_rgctx)
{
	glslrgctx_t *rgctx = _rgctx;
	vrect_t    *rect = &rgctx->glyph_rects[glyphid];
	dstring_t  *batch = rgctx->batch;

}
#endif
void
glsl_Draw_Glyph (int x, int y, int fontid, int glyphid, int c)
{
	if (fontid < 0 || (unsigned) fontid > glsl_fonts.size) {
		return;
	}
	glslfont_t *font = &glsl_fonts.a[fontid];
	font_t     *rfont = font->font;
	vrect_t    *rect = &rfont->glyph_rects[glyphid];

	queue_verts (alpha_2d.program, GL_TRIANGLES, font->texid, 6);

	dstring_t  *batch = glsl_queue;
	int         size = 6 * sizeof (drawvert_t);
	batch->size += size;
	dstring_adjust (batch);
	drawvert_t *verts = (drawvert_t *) (batch->str + batch->size - size);

	float       w = rect->width;
	float       h = rect->height;
	float       u = rect->x;
	float       v = rect->y;
	float       s = 1.0 / rfont->scrap.width;
	float       t = 1.0 / rfont->scrap.height;

	verts[0] = (drawvert_t) {
		.xyst = { x, y, u * s, v * t },
	};
	verts[1] = (drawvert_t) {
		.xyst = { x + w, y, (u + w) * s, v * t },
	};
	verts[2] = (drawvert_t) {
		.xyst = { x + w, y + h, (u + w) * s, (v + h) * t },
	};
	verts[3] = (drawvert_t) {
		.xyst = { x, y, u * s, v * t },
	};
	verts[4] = (drawvert_t) {
		.xyst = { x + w, y + h, (u + w) * s, (v + h) * t },
	};
	verts[5] = (drawvert_t) {
		.xyst = { x, y + h, u * s, (v + h) * t },
	};

	quat_t      color = { VectorExpand (vid.palette + c * 3), 255 };
	QuatScale (color, 1.0f/255.0f, color);
	QuatCopy (color, verts[0].color);
	QuatCopy (color, verts[1].color);
	QuatCopy (color, verts[2].color);
	QuatCopy (color, verts[3].color);
	QuatCopy (color, verts[4].color);
	QuatCopy (color, verts[5].color);
}

void
glsl_Draw_SetClip (int x, int y, int w, int h)
{
}

void
glsl_Draw_ResetClip (void)
{
}
