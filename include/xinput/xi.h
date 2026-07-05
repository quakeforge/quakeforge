#ifndef evdev_inputlib_h
#define evdev_inputlib_h

#include <stdint.h>

typedef struct {
	int         num;	///< The high-level index of the button. Always 0-N
	int         xnum;	///< The low-level index of the button. May be sparse
	int         state;	///< Current state of the button.
} button_t;

typedef struct {
	int         num;	///< The high-level index of the axis. Always 0-N
	int         xnum;	///< The low-level index of the axis. May be sparse
	int         value;	///< Current value of the input axis.
	// relative axes set these to 0
	int         min;	///< Minimum value for the axis (usually constant).
	int         max;	///< Maximum value for the axis (usually constant).
} axis_t;

typedef struct xi_device_s {
	char        name[16];
	uint32_t    user;
	uint16_t    bustype;
	uint16_t    vendor;
	uint16_t    product;
	uint16_t    version;
	int         event_count;

	int         num_buttons;
	int         num_axes;
	button_t    buttons[16];
	axis_t      axes[6];

	void       *data;
	void      (*axis_event) (axis_t *axis, void *data);
	void      (*button_event) (button_t *button, void *data);
} xi_device_t;

int xi_check_input (void);
void xi_close (void);
int xi_init (const char *library, void (*dev_add) (xi_device_t *), void (*dev_rem) (xi_device_t *));

#endif//evdev_inputlib_h
