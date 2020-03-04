#ifndef __qwaq_event_h
#define __qwaq_event_h

typedef enum {
	qe_none,
	qe_key,
	qe_mouse,
	qe_command,		// application level command
} qwaq_etype;

typedef enum {
	qc_valid,
	qc_exit,
	qc_error,
} qwaq_command;

typedef struct qwaq_mevent_s {
	int         x, y;
	int         buttons;
} qwaq_mevent_t;

typedef struct qwaq_message_s {
	qwaq_command command;
} qwaq_message_t;

typedef struct qwaq_event_s {
	qwaq_etype  event_type;
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
