#ifndef __qwaq_app_h
#define __qwaq_app_h

#include <Object.h>

#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/rect.h"

@class Array;
@class Group;
@class TextContext;
@class View;

extern int color_palette[64];

@interface InputApplication: Object
{
	qwaq_event_t event;
	qwaq_command endState;

	Group      *objects;

	TextContext *screen;
	Extent      screenSize;
	int         autocount;
}
-(Extent)size;
-(TextContext *)screen;
-addView:(View *)view;
-run;
@end

extern InputApplication *application;

#endif//__qwaq_app_h
