#ifndef __qwaq_app_h
#define __qwaq_app_h

#include <Object.h>

#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/rect.h"
#include "ruamoko/qwaq/debugger/debugger.h"

@class Array;
@class Group;
@class TextContext;
@class View;
@class Locals;

extern int color_palette[64];

@interface QwaqApplication: Object <DebugGetFile>
{
	qwaq_event_t event;
	qwaq_command endState;

	Group      *objects;

	TextContext *screen;
	Extent      screenSize;
	int         autocount;

	Array      *debuggers;
	Array      *editors;
	Locals     *locals;
}
-(Extent)size;
-(TextContext *)screen;
-addView:(View *)view;
-run;
@end

extern QwaqApplication *application;

#endif//__qwaq_app_h
