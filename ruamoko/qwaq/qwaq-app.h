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

@interface QwaqApplication: Object
{
	qwaq_event_t event;
	qwaq_command endState;

	Group      *objects;

	TextContext *screen;
	Extent      screenSize;
	int         autocount;

	Array      *debuggers;
}
-(Extent)size;
-(TextContext *)screen;
-addView:(View *)view;
-run;
@end

extern QwaqApplication *application;

#endif//__qwaq_app_h
