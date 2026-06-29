#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/plist.h"
#include "QF/pqueue.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/input.h"
#include "QF/input/axis_button.h"
#include "QF/input/event.h"
#include "QF/input/gamepad.h"

#ifdef __linux__
#define PLATFORM "Linux"
#elifdef _WIN32
#define PLATFORM "Windows"
#endif

typedef struct in_ctrlbind_s {
	inm_type_t  type;
	int16_t     axis;
	int16_t     button;
	int8_t      sign;
	int16_t     in_min;
	int16_t     in_max;
	int16_t     out_min;
	int16_t     out_max;
} in_ctrlbind_t;

typedef struct in_ctrlmap_s {
	inm_type_t  type;	// is index for axis, button or hat?
	int         index;
} in_ctrlmap_t;

typedef struct in_gamepad_s {
	const char *name;
	void       *event_data;
	int         ctrlid;
	int         devid;
	int         parent;
	in_ctrlbind_t *axis_binding;
	in_ctrlbind_t *button_binding;
	in_ctrlbind_t *hat_binding;
	in_axis_button_t **axis_buttons;
	in_ctrlmap_t  axis_map[8];
	in_ctrlmap_t  button_map[15];
	int           num_hats;
	int           num_buttons;
	int           num_axes;
	int           num_axis_buttons;
} in_gamepad_t;

static const char gamecontrollerdb[] = {
#embed "gamecontrollerdb.txt" suffix(,)
#embed "gamecontrollerdb_qf.txt" suffix(,)
	0
};

typedef struct mapping_s {
	uint16_t    guid[8];
	uint32_t    name;
	// horrible naming because they match the gamecontrollerdb.txt names
	uint32_t    a;
	uint32_t    b;
	uint32_t    x;
	uint32_t    y;
	uint32_t    leftstick;
	uint32_t    leftshoulder;
	uint32_t    lefttrigger;
	uint32_t    leftx;
	uint32_t    lefty;
	uint32_t    rightstick;
	uint32_t    rightshoulder;
	uint32_t    righttrigger;
	uint32_t    rightx;
	uint32_t    righty;
	uint32_t    back;
	uint32_t    guide;
	uint32_t    start;
	uint32_t    dpleft;
	uint32_t    dpright;
	uint32_t    dpup;
	uint32_t    dpdown;
	uint32_t    platform;
} mapping_t;

typedef struct mapping_field_s {
	const char *name;
	int         offset;
	int         axis;
	int         button;
} mapping_field_t;

#define MAPPING(n, a, b) { \
	.name = #n, \
	.offset = offsetof (mapping_t, n), \
	.axis = a, \
	.button = b, \
}
static mapping_field_t mapping_fields[] = {
	MAPPING (a,            -1,  0),
	MAPPING (b,            -1,  1),
	MAPPING (back,         -1,  8),
	MAPPING (dpdown,        7, 12),
	MAPPING (dpleft,        6, 13),
	MAPPING (dpright,       6, 14),
	MAPPING (dpup,          7, 11),
	MAPPING (guide,        -1, 10),
	MAPPING (leftshoulder, -1,  4),
	MAPPING (leftstick,    -1,  6),
	MAPPING (lefttrigger,   2, -1),
	MAPPING (leftx,         0, -1),
	MAPPING (lefty,         1, -1),
	MAPPING (platform,     -1, -1),
	MAPPING (rightshoulder,-1,  5),
	MAPPING (rightstick,   -1,  7),
	MAPPING (righttrigger,  5, -1),
	MAPPING (rightx,        3, -1),
	MAPPING (righty,        4, -1),
	MAPPING (start,        -1,  9),
	MAPPING (x,            -1,  3),
	MAPPING (y,            -1,  2),
};

static dstring_t mapping_strings = { .mem = &dstring_default_mem };
static hashctx_t *hashctx;
static hashtab_t *mapping_strings_tab;
static hashtab_t *mapping_devices_tab;
static int gamepad_driver_handle = -1;

#define in_gamepad_min 0
#define in_gamepad_max 255

static int
gamepad_ctrlid_compare (const int *a, const int *b, void *data)
{
	return *b - *a;
}

static struct PQUEUE_TYPE(int) gamepad_freed_ctrlids = {
	.q = DARRAY_STATIC_INIT (8),
	.compare = gamepad_ctrlid_compare,
};
static int gamepad_next_ctrlid;

static const char *
mapping_strings_get_key (const void *_offset, void *data)
{
	uint32_t offset = (uintptr_t) _offset;
	return mapping_strings.str + offset;
}

static uintptr_t
mapping_devices_get_hash (const void *_mapping, void *data)
{
	const mapping_t *mapping = _mapping;
	uintptr_t   bustype = mapping->guid[0];
	uintptr_t   vendor = mapping->guid[2];
	uintptr_t   product = mapping->guid[4];
	uintptr_t   version = mapping->guid[6];
	return (bustype << 48) | (vendor << 32) | (product << 16) | version;
}

static bool
mapping_devices_compare (const void *_mapa, const void *_mapb, void *data)
{
	const mapping_t *mapa = _mapa;
	const mapping_t *mapb = _mapb;
	if (mapa->guid[0] != mapb->guid[0]) {//bustype
		return false;
	}
	if (mapa->guid[2] != mapb->guid[2]) {//vendor id
		return false;
	}
	if (mapa->guid[4] != mapb->guid[4]) {//product id
		return false;
	}
	if (mapa->guid[6] != mapb->guid[6]) {//version
		return false;
	}
	return true;
}


static uint32_t
add_string (const char *str)
{
	uint32_t offset = (uintptr_t) Hash_Find (mapping_strings_tab, str);
	if (offset) {
		return offset;
	}
	size_t len = strlen (str) + 1;	// include terminating 0
	offset = mapping_strings.size;
	dstring_append (&mapping_strings, str, len);
	Hash_Add (mapping_strings_tab, (void *) (uintptr_t) offset);
	return offset;
}

static inline byte
tohex (char c)
{
	// assumes valid hex
	return (c & 0xf) + ((c > '9') ? 9 : 0);
}

static int
mapping_field_cmp (const void *_key, const void *_fld)
{
	const char *key = _key;
	const mapping_field_t *fld = _fld;
	return strcmp (key, fld->name);
}

static mapping_field_t *
find_field (const char *name)
{
	return bsearch (name, mapping_fields, countof (mapping_fields),
					sizeof (mapping_fields[0]), mapping_field_cmp);
}

static bool
parse_field (const char *str, byte *mapping)
{
	int         len = strlen (str) + 1;
	char        buf[len];
	strcpy (buf, str);
	if (char *inp = strchr (buf, ':')) {
		*inp++ = 0;
		char *ctrl = buf;
		if (strcmp (ctrl, "platform") == 0) {
			return strcmp (inp, PLATFORM) == 0;
		}
		if (ctrl[0] == '+' || ctrl[0] == '-') {
			ctrl++;
		}
		mapping_field_t *field = find_field (ctrl);
		if (field) {
			uint32_t inp_offs = add_string (inp);
			*(uint32_t *)(mapping + field->offset) = inp_offs;
		}
	}
	return true;
}

static void
in_gamepad_init (void *data)
{
}

static void
in_gamepad_shutdown (void *data)
{
	free (mapping_strings.str);
	Hash_DelTable (mapping_strings_tab);
	Hash_DelTable (mapping_devices_tab);
}

static void
in_gamepad_set_device_event_data (void *device, void *event_data, void *data)
{
	in_gamepad_t *gamepad = device;
	gamepad->event_data = event_data;
}

static void *
in_gamepad_get_device_event_data (void *device, void *data)
{
	in_gamepad_t *gamepad = device;
	return gamepad->event_data;
}

static int
convert_axis (int value, in_ctrlbind_t bind)
{
	int64_t domain = abs (bind.in_max - bind.in_min);
	int64_t range = bind.out_max - bind.out_min;
	if (domain == 0 || range == 0) {
		return value;
	}
	value = ((value - bind.in_min) * range) / domain;
	int base = bind.sign < 0 ? bind.out_max : bind.out_min;
	return value + base;
}

static int
button_to_axis (int state, in_ctrlbind_t bind)
{
	return state ? bind.out_max : bind.out_min;
}

static in_axisinfo_t
calc_axis_info (in_gamepad_t *gamepad, in_ctrlmap_t map, int num)
{
	in_axisinfo_t info = { .axis = -1 };
	in_ctrlbind_t bind;
	switch (map.type) {
		case inm_none:
			break;
		case inm_abs_axis:
		case inm_rel_axis:
			bind = gamepad->axis_binding[map.index];
			in_axisinfo_t axis;
			IN_GetAxisInfo (gamepad->parent, map.index, &axis);
			info = (in_axisinfo_t) {
				.deviceid = gamepad->devid,
				.axis = num,
				.value = convert_axis (axis.value, bind),
				.min = bind.sign ? 0 : in_gamepad_min,
				.max = bind.sign >= 0 ? in_gamepad_max : in_gamepad_min,
			};
			break;
		case inm_button:
			bind = gamepad->button_binding[map.index];
			in_buttoninfo_t button;
			IN_GetButtonInfo (gamepad->parent, map.index, &button);
			info = (in_axisinfo_t) {
				.deviceid = gamepad->devid,
				.axis = num,
				.value = button_to_axis (button.state, bind),
				.min = bind.out_min,
				.max = bind.out_max,
			};
			break;
		case inm_hat:
			//FIXME
			break;
	}
	return info;
}

static in_buttoninfo_t
calc_button_info (in_gamepad_t *gamepad, in_ctrlmap_t map, int num)
{
	in_buttoninfo_t info = { .button = -1 };
	in_ctrlbind_t bind;
	switch (map.type) {
		case inm_none:
			break;
		case inm_abs_axis:
		case inm_rel_axis:
			bind = gamepad->axis_binding[map.index];
			in_axisinfo_t axis;
			IN_GetAxisInfo (gamepad->parent, map.index, &axis);
			auto ab = gamepad->axis_buttons[bind.button];
			info = (in_buttoninfo_t) {
				.deviceid = gamepad->devid,
				.button = num,
				.state = IN_AxisButton_Test (ab, axis.value, num),
			};
			break;
		case inm_button:
			in_buttoninfo_t button;
			IN_GetButtonInfo (gamepad->parent, map.index, &button);
			info = (in_buttoninfo_t) {
				.deviceid = gamepad->devid,
				.button = num,
				.state = button.state,
			};
			break;
		case inm_hat:
			//FIXME
			break;
	}
	return info;
}

static void
in_gamepad_axis_info (void *data, void *device, in_axisinfo_t *axes,
					  int *numaxes)
{
	in_gamepad_t *gamepad = device;
	if (!axes) {
		*numaxes = countof (gamepad->axis_map);
		return;
	}
	if ((unsigned) *numaxes > countof (gamepad->axis_map)) {
		*numaxes = countof (gamepad->axis_map);
	}
	for (int i = 0; i < *numaxes; i++) {
		axes[i] = calc_axis_info (gamepad, gamepad->axis_map[i], i);
	}
}

static void
in_gamepad_button_info (void *data, void *device, in_buttoninfo_t *buttons,
						int *numbuttons)
{
	in_gamepad_t *gamepad = device;
	if (!buttons) {
		*numbuttons = countof (gamepad->button_map);
		return;
	}
	if ((unsigned) *numbuttons > countof (gamepad->button_map)) {
		*numbuttons = countof (gamepad->button_map);
	}
	for (int i = 0; i < *numbuttons; i++) {
		buttons[i] = calc_button_info (gamepad, gamepad->button_map[i], i);
	}
}

static const char *in_gampad_axis_names[] = {
	"left_x",
	"left_y",
	"left_trigger",
	"right_x",
	"right_y",
	"right_trigger",
	"dpad_horizontal",
	"dpad_vertical",
};
static_assert (countof (in_gampad_axis_names)
			   == countof (((in_gamepad_t *)0)->axis_map));

static const char *in_gampad_button_names[] = {
	"south",
	"east",
	"north",
	"west",
	"left_shoulder",
	"right_shoulder",
	"left_stick",
	"right_stick",
	"back",
	"start",
	"guide",
	"dpad_up",
	"dpad_down",
	"dpad_left",
	"dpad_right",
};
static_assert (countof (in_gampad_button_names)
			   == countof (((in_gamepad_t *)0)->button_map));

static const char *
in_gamepad_get_axis_name (void *data, void *device, int axis_num)
{
	if ((unsigned) axis_num >= countof (in_gampad_axis_names)) {
		return nullptr;
	}
	return in_gampad_axis_names[axis_num];
}

static const char *
in_gamepad_get_button_name (void *data, void *device, int button_num)
{
	if ((unsigned) button_num >= countof (in_gampad_button_names)) {
		return nullptr;
	}
	return in_gampad_button_names[button_num];
}

static int
in_gamepad_get_axis_num (void *data, void *device, const char *axis_name)
{
	for (unsigned i = 0; i < countof (in_gampad_axis_names); i++) {
		if (strcasecmp (in_gampad_axis_names[i], axis_name) == 0) {
			return i;
		}
	}
	return -1;
}

static int
in_gamepad_get_button_num (void *data, void *device, const char *button_name)
{
	for (unsigned i = 0; i < countof (in_gampad_button_names); i++) {
		if (strcasecmp (in_gampad_button_names[i], button_name) == 0) {
			return i;
		}
	}
	return -1;
}

static int
in_gamepad_get_axis_info (void *data, void *device, int axis_num,
						  in_axisinfo_t *info)
{
	in_gamepad_t *gamepad = device;
	if ((unsigned) axis_num >= countof (gamepad->axis_map)) {
		return 0;
	}
	*info = calc_axis_info (gamepad, gamepad->axis_map[axis_num], axis_num);
	return 1;
}

static int
in_gamepad_get_button_info (void *data, void *device, int button_num,
							in_buttoninfo_t *info)
{
	in_gamepad_t *gamepad = device;
	if ((unsigned) button_num >= countof (gamepad->button_map)) {
		return 0;
	}
	*info = calc_button_info (gamepad, gamepad->button_map[button_num],
							  button_num);
	return 1;
}

static in_driver_t in_gamepad_driver = {
	.init = in_gamepad_init,
	.shutdown = in_gamepad_shutdown,
	.set_device_event_data = in_gamepad_set_device_event_data,
	.get_device_event_data = in_gamepad_get_device_event_data,

	.axis_info = in_gamepad_axis_info,
	.button_info = in_gamepad_button_info,

	.get_axis_name = in_gamepad_get_axis_name,
	.get_button_name = in_gamepad_get_button_name,

	.get_axis_num = in_gamepad_get_axis_num,
	.get_button_num = in_gamepad_get_button_num,

	.get_axis_info = in_gamepad_get_axis_info,
	.get_button_info = in_gamepad_get_button_info,
};

void
IN_Gamepad_Init (void)
{
	gamepad_driver_handle = IN_RegisterDriver (&in_gamepad_driver, 0);
	mapping_strings_tab = Hash_NewTable (251, mapping_strings_get_key,
										 nullptr, nullptr, &hashctx);
	dstring_append (&mapping_strings, "", 1);

	mapping_devices_tab = Hash_NewTable (251, nullptr, nullptr, nullptr,
										 &hashctx);
	Hash_SetHashCompare (mapping_devices_tab, mapping_devices_get_hash,
						 mapping_devices_compare);

	for (int i = 0; i < 8; i++) {
		add_string (va ("a%d", i));
	}
	for (int i = 0; i < 16; i++) {
		add_string (va ("b%d", i));
	}
	for (int i = 0; i < 4; i++) {
		add_string (va ("h0.%d", 1 << i));
	}
	for (const char *s = gamecontrollerdb, *e; *s; s = e + (*e != 0)) {
		bool is_comment = false;
		bool is_blank = true;
		for (e = s; *e && *e != '\n'; e++) {
			if (is_blank && *e == '#') {
				is_comment = true;
			}
			if (*e > ' ') {
				is_blank = false;
			}
		}
		if (is_comment || is_blank) {
			continue;
		}
		auto item = PL_ParseCSV_len (s, e - s, &hashctx);
		if (PL_A_NumObjects (item) < 2) {
			continue;
		}
		byte guid_bytes[16] = {};
		const char *guid_str = PL_String (PL_ObjectAtIndex (item, 0));
		size_t len = strlen (guid_str);
		for (size_t i = 0; i < len && i/2 < countof (guid_bytes); i++) {
			guid_bytes[i / 2] = (guid_bytes[i / 2] << 4) + tohex(guid_str[i]);
		}
		mapping_t mapping = {};
		for (int i = 0; i < 8; i++) {
			mapping.guid[i] = guid_bytes[i * 2] + (guid_bytes[i * 2 + 1] << 8);
		}
		bool save = true;
		for (int i = 2; i < PL_A_NumObjects (item); i++) {
			auto str = PL_String (PL_ObjectAtIndex (item, i));
			if (!parse_field (str, (byte *) &mapping)) {
				save = false;
				break;
			}
		}
		if (save) {
			mapping.name = add_string (PL_String (PL_ObjectAtIndex (item, 1)));
			mapping_t *map = malloc (sizeof (mapping_t));
			*map = mapping;
			Hash_AddElement (mapping_devices_tab, map);
		}
		PL_Release (item);
	}
}

static const char *
get_field_name (const mapping_field_t *field, const mapping_t *mapping)
{
	uint32_t offset = field->offset;
	auto stroffs = *(uint32_t *) ((const byte *) mapping + offset);
	return mapping_strings.str + stroffs;
}

static int
calc_threshold (in_gamepad_t *gamepad, in_mapping_t inp)
{
	in_axisinfo_t axis;
	IN_GetAxisInfo (gamepad->parent, inp.index, &axis);

	int threshold = 0;
	if (inp.sign < 0) {
		int mid = (axis.min + axis.max) / 2;
		threshold = (axis.min - (mid + 1)) / 2;
	} else if (inp.sign > 0) {
		int mid = (axis.min + axis.max) / 2;
		threshold = (axis.max - (mid - 1)) / 2;
	} else {
		threshold = (axis.max - axis.min) / 2;
	}
	return threshold;
}

static void
create_axis_bindings (in_gamepad_t *gamepad, mapping_t *mapping, in_device_t *device)
{
	auto driver = IN_GetDriver (device->driverid);
	void *driver_data = IN_GetDriverData (device->driverid);
	int  num_axes = gamepad->num_axes;

	int axis_button_counts[num_axes + 1] = {};
	for (uint32_t i = 0; i < countof (mapping_fields); i++) {
		auto field = &mapping_fields[i];
		if (field->button < 0) {
			continue;
		}
		auto inp_name = get_field_name (field, mapping);
		auto inp = driver->gamepad_mapping (driver_data, device->device,
											inp_name);
		if (inp.type == inm_abs_axis) {
			axis_button_counts[inp.index]++;
		}
	}
	int max_buttons = 0;
	int num_axis_buttons = 0;
	int axis_button_inds[num_axes + 1] = {};
	for (int i = 0; i < num_axes; i++) {
		axis_button_inds[i] = -1;
		if (axis_button_counts[i] > max_buttons) {
			max_buttons = axis_button_counts[i];
		}
		if (axis_button_counts[i]) {
			axis_button_inds[i] = num_axis_buttons++;
		}
		axis_button_counts[i] = 0;
	}

	if (max_buttons && num_axis_buttons) {
		in_ab_state_t ab_buttons[max_buttons * num_axis_buttons] = {};
		int axis_inds[num_axis_buttons];

		for (uint32_t i = 0; i < countof (mapping_fields); i++) {
			auto field = &mapping_fields[i];
			if (field->button < 0) {
				continue;
			}
			auto inp_name = get_field_name (field, mapping);
			auto inp = driver->gamepad_mapping (driver_data, device->device,
												inp_name);
			if (inp.type == inm_abs_axis && axis_button_inds[inp.index] >= 0) {
				int count = axis_button_counts[inp.index];
				int ind = axis_button_inds[inp.index] * max_buttons;
				auto buttons = &ab_buttons[ind];
				axis_inds[axis_button_inds[inp.index]] = inp.index;
				buttons[count++] = (in_ab_state_t) {
					.threshold = calc_threshold (gamepad, inp),
					.button = field->button,
				};
				axis_button_counts[inp.index] = count;
			}
		}
		size_t size = sizeof (in_axis_button_t *[num_axis_buttons]);
		gamepad->axis_buttons = malloc (size);
		gamepad->num_axis_buttons = num_axis_buttons;
		for (int i = 0; i < num_axis_buttons; i++) {
			auto buttons = &ab_buttons[i * max_buttons];
			int count = axis_button_counts[axis_inds[i]];
			auto ab = IN_AxisButton_Create (gamepad->devid, count, buttons);
			gamepad->axis_buttons[i] = ab;
		}
	}

	for (uint32_t i = 0; i < countof (mapping_fields); i++) {
		auto field = &mapping_fields[i];
		auto inp_name = get_field_name (field, mapping);
		auto inp = driver->gamepad_mapping (driver_data, device->device,
											inp_name);
		if (inp.type == inm_abs_axis || inp.type == inm_rel_axis) {
			in_axisinfo_t axis;
			IN_GetAxisInfo (gamepad->parent, inp.index, &axis);
			auto binding = &gamepad->axis_binding[inp.index];
			bool dp = strncmp (field->name, "dp", 2) == 0;
			bool but = field->button >= 0;
			*binding = (in_ctrlbind_t) {
				.type = inp.type,
				.axis = field->axis,
				.button = but ? axis_button_inds[inp.index] : -1,
				.in_min = axis.min,
				.in_max = axis.max,
				.out_min = dp ? -1 : in_gamepad_min,
				.out_max = dp ?  1 : in_gamepad_max,
			};
			if (but) {
				gamepad->button_map[field->button] = (in_ctrlmap_t) {
					.type = inp.type,
					.index = inp.index,
				};
			}
		}
	}
}

in_gamepad_t *
IN_Gamepad_Add (in_devid_t devid, int deviceid)
{
	auto device = IN_GetDevice (deviceid);
	auto driver = IN_GetDriver (device->driverid);
	if (!driver->gamepad_mapping) {
		return nullptr;
	}

	mapping_t key = {
		.guid[0] = devid.bustype,
		.guid[2] = devid.vendor,
		.guid[4] = devid.product,
		.guid[6] = devid.version,
	};
	mapping_t *mapping = Hash_FindElement (mapping_devices_tab, &key);
	if (!mapping) {
		Sys_MaskPrintf (SYS_input, "no mapping for: %04x %04x %04x %04x\n",
						key.guid[0], key.guid[2], key.guid[4], key.guid[6]);
		return nullptr;
	}
	Sys_MaskPrintf (SYS_input, "found mapping %p\n", mapping);
	Sys_MaskPrintf (SYS_input, "    %04x %04x %04x %04x %s\n",
					key.guid[0], key.guid[2], key.guid[4], key.guid[6],
					mapping_strings.str + mapping->name);

	void *driver_data = IN_GetDriverData (device->driverid);

	int num_axes, num_buttons, num_hats = 0;
	IN_AxisInfo (deviceid, nullptr, &num_axes);
	IN_ButtonInfo (deviceid, nullptr, &num_buttons);

	int ctrlid;
	if (PQUEUE_IS_EMPTY (&gamepad_freed_ctrlids)) {
		ctrlid = gamepad_next_ctrlid++;
	} else {
		ctrlid = PQUEUE_REMOVE (&gamepad_freed_ctrlids);
	}
	in_gamepad_t *gamepad = malloc (sizeof (in_gamepad_t));
	*gamepad = (in_gamepad_t) {
		.name = nva ("gamepad:%d", ctrlid),
		.ctrlid = ctrlid,
		.parent = deviceid,
		.axis_binding = malloc (num_axes * sizeof (in_ctrlbind_t)),
		.button_binding = malloc (num_buttons * sizeof (in_ctrlbind_t)),
		.hat_binding = malloc (num_hats * sizeof (in_ctrlbind_t)),
		.num_axes = num_axes,
		.num_buttons = num_buttons,
		.num_hats = num_hats,
	};
	memset (gamepad->axis_binding, -1, num_axes * sizeof (in_ctrlbind_t));
	memset (gamepad->button_binding, -1, num_buttons * sizeof (in_ctrlbind_t));
	gamepad->devid = IN_AddDevice (gamepad_driver_handle, gamepad,
								   gamepad->name, gamepad->name);

	create_axis_bindings (gamepad, mapping, device);

	for (uint32_t i = 0; i < countof (mapping_fields); i++) {
		auto field = &mapping_fields[i];
		if (field->axis < 0 && field->button < 0) {
			continue;
		}
		auto inp_name = get_field_name (field, mapping);
		auto inp = driver->gamepad_mapping (driver_data, device->device,
											inp_name);
		in_ctrlbind_t *binding = nullptr;
		switch (inp.type) {
			case inm_none:
				continue;
			case inm_abs_axis:
			case inm_rel_axis:
				// already done
				continue;
			case inm_button:
				binding = &gamepad->button_binding[inp.index];
				break;
			case inm_hat:
				binding = &gamepad->hat_binding[inp.index];
				break;
		}
		*binding = (in_ctrlbind_t) {
			.type = inp.type,
			.axis = field->axis,
			.button = field->button,
			.sign = inp.sign,
		};
		if (inp.sign) {
			if (inp.type == inm_button) {
				binding->out_min = 0;
				binding->out_max = inp.sign > 0
								 ? in_gamepad_max
								 : in_gamepad_min;
			} else {
				in_axisinfo_t axis;
				IN_GetAxisInfo (gamepad->parent, inp.index, &axis);
				binding->in_min = inp.sign >= 0 ? axis.min : axis.max;
				binding->in_max = inp.sign >= 0 ? axis.max : axis.min;
				if (strncmp (field->name, "dp", 2) == 0) {
					binding->out_min = -1;
					binding->out_max =  1;
				} else {
					binding->out_min = in_gamepad_min;
					binding->out_max = in_gamepad_max;
				}
			}
		} else {
			if (inp.type == inm_button) {
				binding->out_min = 0;
				if (strncmp (field->name, "dp", 2) == 0) {
					int c = field->name[2];
					binding->out_max = c == 'u' || c == 'l' ? -1 : 1;
				} else {
					binding->out_max = in_gamepad_max;
				}
			} else {
				in_axisinfo_t axis;
				IN_GetAxisInfo (gamepad->parent, inp.index, &axis);
				binding->in_min = axis.min;
				binding->in_max = axis.max;
				if (strncmp (field->name, "dp", 2) == 0) {
					binding->out_min = -1;
					binding->out_max =  1;
				} else {
					binding->out_min = in_gamepad_min;
					binding->out_max = in_gamepad_max;
				}
			}
		}
		if (field->axis >= 0) {
			gamepad->axis_map[field->axis] = (in_ctrlmap_t) {
				.type = inp.type,
				.index = inp.index,
			};
		}
		if (field->button >= 0) {
			gamepad->button_map[field->button] = (in_ctrlmap_t) {
				.type = inp.type,
				.index = inp.index,
			};
		}
	}
	return gamepad;
}

void
IN_Gamepad_Remove (in_gamepad_t *gamepad)
{
	PQUEUE_INSERT (&gamepad_freed_ctrlids, gamepad->ctrlid);
	IN_RemoveDevice (gamepad->devid);
	free (gamepad->axis_binding);
	free (gamepad->button_binding);
	free (gamepad->hat_binding);
	free ((char *) gamepad->name);
	free (gamepad);
}

void
IN_Gamepad_Event (in_gamepad_t *gamepad, IE_event_t *ie_event)
{
	if (ie_event->type == ie_axis) {
		int value = ie_event->axis.value;
		auto binding = gamepad->axis_binding[ie_event->axis.axis];
		IE_event_t event = {
			.when = ie_event->when,
		};
		if (binding.axis >= 0) {
			event.type = ie_axis;
			event.axis = (IE_axis_event_t) {
				.data = gamepad->event_data,
				.devid = gamepad->devid,
				.axis = binding.axis,
				.value = convert_axis (value, binding),
			};
			IE_Send_Event (&event);
		}
		if (binding.button >= 0) {
			auto ab = gamepad->axis_buttons[binding.button];
			IN_AxisButton_Event (ab, value);
		}
	} else if (ie_event->type == ie_button) {
		auto binding = gamepad->button_binding[ie_event->button.button];
		IE_event_t event = {
			.when = ie_event->when,
		};
		if (binding.axis >= 0) {
			event.type = ie_axis;
			event.axis = (IE_axis_event_t) {
				.data = gamepad->event_data,
				.devid = gamepad->devid,
				.axis = binding.axis,
				.value = button_to_axis (ie_event->button.state, binding),
			};
			IE_Send_Event (&event);
		}
		if (binding.button >= 0) {
			event.type = ie_button;
			event.button = (IE_button_event_t) {
				.data = gamepad->event_data,
				.devid = gamepad->devid,
				.button = binding.button,
				.state = ie_event->button.state,
			};
			IE_Send_Event (&event);
		}
	}
}
