#ifndef __qwaq_input_h
#define __qwaq_input_h

#include "QF/input.h"

typedef struct qwaq_devinfo_s {
	int         devid;
#ifdef __QFCC__
	string      name;
	string      id;
#else
	pr_string_t name;
	pr_string_t id;
#endif
	int         numaxes;
#ifdef __QFCC__
	in_axisinfo_t *axes;
#else
	pr_ptr_t    axes;
#endif
	int         numbuttons;
#ifdef __QFCC__
	in_axisinfo_t *buttons;
#else
	pr_ptr_t    buttons;
#endif
} qwaq_devinfo_t;

#ifdef __QFCC__

#include <Object.h>

#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/rect.h"


@class Array;
@class Group;
@class TextContext;
@class View;

@interface QwaqInput: Object
{
}
@end

void init_input (void);
void send_connected_devices (void);
qwaq_devinfo_t *get_device_info (int devid);

#endif

#endif//__qwaq_input_h
