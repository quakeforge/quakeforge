/*
	XYView.h

	X/Y "perspective" viewer class definitions

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

#include <QF/mathlib.h>

#import <AppKit/AppKit.h>
#import "SetBrush.h"

#define	MINSCALE	0.125
#define	MAXSCALE	2.0

typedef enum {dr_wire, dr_flat, dr_texture} drawmode_t;

@interface XYView:  NSView
{
	NSRect		realbounds, newrect, combinedrect;
	NSPoint		midpoint;
	int			gridsize;
	float		scale;

// for textured view
	int			xywidth, xyheight;
	float		*xyzbuffer;
	unsigned	*xypicbuffer;

	drawmode_t	drawmode;

// UI links
	id			mode_radio_i;	
}

- (float)currentScale;

- setModeRadio: m;

- drawMode: sender;
- setDrawMode: (drawmode_t) mode;

- newSuperBounds;
- newRealBounds: (NSRect) nb;

- addToScrollRange: (float) x : (float) y;
- setOrigin: (NSPoint) pt scale: (float) sc;
- centerOn: (vec3_t)org;

- drawMode: sender;

- superviewChanged;

- (int) gridsize;
- (float) snapToGrid: (float) f;

@end

extern XYView	*xyView;

extern vec3_t	xy_viewnormal;		// v_forward for xy view
extern float	xy_viewdist;		// clip behind this plane

extern NSRect	xy_draw_rect;

void linestart (float r, float g, float b);
void lineflush (void);
void linecolor (float r, float g, float b);

void XYmoveto (vec3_t pt);
void XYlineto (vec3_t pt);
