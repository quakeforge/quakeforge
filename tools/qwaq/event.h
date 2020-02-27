#ifndef __qwaq_event_h
#define __qwaq_event_h

typedef enum {
	qe_idle,
	qe_key,
	qe_mouse,
} qwaq_etype;

// right now, this is just a copy of ncurses MEVENT, but all int
typedef struct qwaq_mevent_s {
	int         id;			// XXX does it matter?
	int         x, y, z;	// z? what?
	int         buttons;
} qwaq_mevent_t;

typedef struct qwaq_event_s {
	qwaq_etype  event_type;
	union {
		int         key;
		qwaq_mevent_t mouse;
	} e;
} qwaq_event_t;

#endif//__qwaq_event_h
