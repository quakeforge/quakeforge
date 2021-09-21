#ifndef __qwaq_ui_event_h
#define __qwaq_ui_event_h

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
	qe_shift     = 1,
	qe_control   = 4,
} qwaq_key_shift;

typedef enum {
	qe_command   = 0x0200,		// application level command
	qe_broadcast = 0x0400,
	qe_resize    = 0x0800,		// screen resized

	qe_dev_add   = 0x1000,
	qe_dev_rem   = 0x2000,
	qe_axis      = 0x4000,
	qe_button    = 0x8000,
} qwaq_message_event;

typedef enum {
	qe_none    = 0x0000,
	qe_mouse   = 0x001f,
	qe_key     = 0x0020,
	qe_system  = 0x01c0,	//FIXME this isn't very manageable
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

typedef struct qwaq_kevent_s {
	int         code;
	int         shift;
} qwaq_kevent_t;

typedef struct qwaq_resize_s {
	int         width;
	int         height;
} qwaq_resize_t;

typedef union qwaq_message_s {
	int         int_val;
	float       float_val;
	float       vector_val[4];	// vector and quaternion
	int         ivector_val[4];
	double      double_val;
#ifdef __QFCC__
	void       *pointer_val;
	string      string_val;
#else
	pointer_t   pointer_val;
	string_t    string_val;
#endif
} qwaq_message_t;

typedef struct qwaq_event_s {
	int         what;
	int         __pad;
	double      when;		// NOTE: 1<<32 based
	union {
		qwaq_kevent_t key;
		qwaq_mevent_t mouse;
		qwaq_message_t message;
		qwaq_resize_t resize;
	};
} qwaq_event_t;

#endif//__qwaq_ui_event_h
