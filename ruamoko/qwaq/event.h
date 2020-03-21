#ifndef __qwaq_event_h
#define __qwaq_event_h

typedef enum {
	qe_mousedown = 0x0001,
	qe_mouseup   = 0x0002,
	qe_mouseclick= 0x0004,
	qe_mousemove = 0x0008,
	qe_mouseauto = 0x0010,
} qwaq_mouse_event;

typedef enum {
	qe_keydown   = 0x0020,
} qwaq_key_event;

typedef enum {
	qe_command   = 0x0200,		// application level command
	qe_broadcast = 0x0400,
} qwaq_message_event;

typedef enum {
	qe_none    = 0x0000,
	qe_mouse   = 0x001f,
	qe_key     = 0x0020,
	qe_system  = 0x01c0,
	qe_message = 0xfe00,

	qe_focused = qe_key | qe_command,
	qe_positional = qe_mouse,
} qwaq_event_mask;

typedef enum {
	qc_valid,
	qc_exit,
	qc_error,
} qwaq_command;

typedef struct qwaq_mevent_s {
	int         x, y;
	int         buttons;	// current button state
	int         click;
} qwaq_mevent_t;

typedef struct qwaq_message_s {
	qwaq_command command;
} qwaq_message_t;

typedef struct qwaq_event_s {
	int         what;
	double      when;		// NOTE: 1<<32 based
	union {
		int         key;
		qwaq_mevent_t mouse;
		qwaq_message_t message;
	};
} qwaq_event_t;

#ifdef __QFCC__	// don't want C gcc to see this :)
@protocol HandleEvent
-handleEvent: (struct qwaq_event_s *) event;
@end

@protocol HandleFocusedEvent <HandleEvent>
-takeFocus;
-loseFocus;
@end

@protocol HandleMouseEvent <HandleEvent>
-(struct Rect_s *)getRect;
@end

#endif

#endif//__qwaq_event_h
