#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <linux/input.h>
#include <linux/joystick.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/sys.h"

#include "evdev/hotplug.h"
#include "evdev/inputlib.h"

static const char *devinput_path = "/dev/input";
static device_t *devices;
void (*device_add) (device_t *);
void (*device_remove) (device_t *);

static void
setup_buttons (device_t *dev)
{
	int         i, j, len;
	unsigned char buf[1024];
	button_t   *button;

	dev->max_button = -1;
	dev->num_buttons = 0;
	dev->button_map = 0;
	dev->buttons = 0;
	len = ioctl (dev->fd, EVIOCGBIT (EV_KEY, sizeof (buf)), buf);
	for (i = 0; i < len; i++) {
		//Sys_Printf("%c%02x", !(i % 16) ? '\n': !(i % 8) ? '-' : ' ', buf[i]);
		for (j = 0; j < 8; j++) {
			if (buf[i] & (1 << j)) {
				dev->num_buttons++;
				dev->max_button = i * 8 + j;
			}
		}
	}
	//Sys_Printf("\n");
	dev->button_map = malloc ((dev->max_button + 1) * sizeof (int));
	dev->buttons = malloc (dev->num_buttons * sizeof (button_t));
	for (i = 0, button = dev->buttons; i < len; i++) {
		for (j = 0; j < 8; j++) {
			int         button_ind = i * 8 + j;
			if (buf[i] & (1 << j)) {
				button->num = button - dev->buttons;
				button->evnum = button_ind;
				button->state = 0;
				dev->button_map[button_ind] = button->num;

				button++;
			} else {
				if (button_ind <= dev->max_button) {
					dev->button_map[button_ind] = -1;
				}
			}
		}
	}
	len = ioctl (dev->fd, EVIOCGKEY (sizeof (buf)), buf);
	for (i = 0; i < dev->num_buttons; i++) {
		int         key = dev->buttons[i].evnum;
		dev->buttons[i].state = !!(buf[key / 8] & (1 << (key % 8)));
	}
}

static int
count_axes (const unsigned char *buf, int len, int *max_axis)
{
	int         count = 0;
	int         i, j;

	for (i = 0; i < len; i++) {
		for (j = 0; j < 8; j++) {
			if (buf[i] & (1 << j)) {
				count++;
				*max_axis = i * 8 + j;
			}
		}
	}
	return count;
}

static void
abs_info (device_t *dev, int axis_ind, axis_t *axis)
{
	struct input_absinfo absinfo;
	ioctl (dev->fd, EVIOCGABS(axis_ind), &absinfo);
	axis->value = absinfo.value;
	axis->min = absinfo.minimum;
	axis->max = absinfo.maximum;

}

static void
rel_info (device_t *dev, int axis_ind, axis_t *axis)
{
	// relative axes are marked by having 0 min/max
	axis->value = 0;
	axis->min = 0;
	axis->max = 0;
}

static void
map_axes (const unsigned char *buf, int len, device_t *dev,
		  int max_axis, int *axis_map, axis_t *first_axis,
		  void (*info)(device_t*, int, axis_t *))
{
	int         i, j;
	axis_t     *axis;

	for (i = 0, axis = first_axis; i < len; i++) {
		for (j = 0; j < 8; j++) {
			int         axis_ind = i * 8 + j;
			if (buf[i] & (1 << j)) {
				axis->num = axis - dev->axes;
				axis->evnum = axis_ind;
				axis_map[axis_ind] = axis->num;
				info (dev, axis_ind, axis);
				axis++;
			} else {
				if (axis_ind <= max_axis) {
					axis_map[axis_ind] = -1;
				}
			}
		}
	}
}

static void
setup_axes (device_t *dev)
{
	int         alen, rlen;
	unsigned char abuf[1024];
	unsigned char rbuf[1024];

	dev->max_abs_axis = -1;
	dev->max_rel_axis = -1;
	dev->num_axes = 0;
	dev->abs_axis_map = 0;
	dev->rel_axis_map = 0;
	dev->axes = 0;

	alen = ioctl (dev->fd, EVIOCGBIT (EV_ABS, sizeof (abuf)), abuf);
	rlen = ioctl (dev->fd, EVIOCGBIT (EV_REL, sizeof (rbuf)), rbuf);

	dev->num_abs_axes = count_axes (abuf, alen, &dev->max_abs_axis);
	dev->num_rel_axes = count_axes (rbuf, alen, &dev->max_rel_axis);

	dev->num_axes = dev->num_abs_axes + dev->num_rel_axes;

	dev->abs_axis_map = malloc ((dev->max_abs_axis + 1) * sizeof (int));
	dev->rel_axis_map = malloc ((dev->max_rel_axis + 1) * sizeof (int));

	dev->axes = malloc (dev->num_axes * sizeof (axis_t));
	map_axes (abuf, alen, dev, dev->max_abs_axis, dev->abs_axis_map,
			  dev->axes, abs_info);
	map_axes (rbuf, rlen, dev, dev->max_rel_axis, dev->rel_axis_map,
			  dev->axes + dev->num_abs_axes, rel_info);
}

static void device_created (const char *name);
static void device_deleted (const char *name);

#define get_string(fd, ioctlid, dstr)	\
	({																	\
		int         size;												\
		while ((size = ioctl (fd, ioctlid (dstr->truesize), dstr->str))	\
			   == (int) dstr->truesize) {								\
			dstr->size = dstr->truesize + 1024;							\
			dstring_adjust (dstr);										\
		}																\
		dstr->size = size <= 0 ? 1 : size;								\
		dstr->str[dstr->size - 1] = 0;									\
		dstr->str;														\
	})

static int
check_device (const char *path)
{
	device_t   *dev;
	int         fd;

	fd = open (path, O_RDWR);
	if (fd == -1)
		return -1;

	dev = malloc (sizeof (device_t));
	dev->next = devices;
	dev->prev = &devices;
	if (devices) {
		devices->prev = &dev->next;
	}
	devices = dev;

	dev->path = strdup (path);
	dev->fd = fd;

	dstring_t  *buff = dstring_newstr ();
	dev->name = strdup (get_string (fd, EVIOCGNAME, buff));
	dev->phys = strdup (get_string (fd, EVIOCGPHYS, buff));
	dev->uniq = strdup (get_string (fd, EVIOCGUNIQ, buff));
	dstring_delete (buff);

	setup_buttons(dev);
	setup_axes(dev);

	dev->event_count = 0;

	dev->data = 0;
	dev->axis_event = 0;
	dev->button_event = 0;


	//Sys_Printf ("%s:\n", path);
	//Sys_Printf ("\tname: %s\n", dev->name);
	//Sys_Printf ("\tbuttons: %d\n", dev->num_buttons);
	//Sys_Printf ("\taxes: %d\n", dev->num_axes);

	if (device_add) {
		device_add (dev);
	}

	return fd;
}

/*static const char *event_codes[] = {
	"EV_SYN",
	"EV_KEY",
	"EV_REL",
	"EV_ABS",
	"EV_MSC",
	"EV_SW",
	0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	"EV_LED",
	"EV_SND",
	"EV_REP",
	"EV_FF",
	"EV_PWR",
	"EV_FF_STATUS",
};*/

static void
read_device_input (device_t *dev)
{
	struct input_event event;
	button_t   *button;
	axis_t     *axis;
	//int         i;

	// zero motion counters for relative axes
	//for (i = dev->num_abs_axes; i < dev->num_axes; i++) {
	//	dev->axes[i].value = 0;
	//}

	while (1) {
		if (read (dev->fd, &event, sizeof (event)) < 0) {
			perror(dev->name);
			dev->fd = -1;
			return;
		}
		//const char *ev = event_codes[event.type];
		//Sys_Printf ("%6d(%s) %6d %6x\n", event.type, ev ? ev : "?", event.code, event.value);
		switch (event.type) {
			case EV_SYN:
				dev->event_count++;
				return;
			case EV_KEY:
				button = &dev->buttons[dev->button_map[event.code]];
				button->state = event.value;
				if (dev->button_event) {
					dev->button_event (button, dev->data);
				}
				break;
			case EV_ABS:
				axis = &dev->axes[dev->abs_axis_map[event.code]];
				axis->value = event.value;
				if (dev->axis_event) {
					dev->axis_event (axis, dev->data);
				}
				break;
			case EV_MSC:
				break;
			case EV_REL:
				axis = &dev->axes[dev->rel_axis_map[event.code]];
				//Sys_Printf ("EV_REL %6d %6x %6d %p\n", event.code, event.value,
				//		dev->rel_axis_map[event.code], axis);
				axis->value = event.value;
				if (dev->axis_event) {
					dev->axis_event (axis, dev->data);
				}
				break;
			case EV_SW:
			case EV_LED:
			case EV_SND:
			case EV_REP:
			case EV_FF:
			case EV_PWR:
			case EV_FF_STATUS:
				//Sys_Printf ("%6d %6d %6x\n", event.type, event.code, event.value);
				break;
		}
	}
}

void
inputlib_add_select (fd_set *fdset, int *maxfd)
{
	inputlib_hotplug_add_select (fdset, maxfd);

	for (device_t *dev = devices; dev; dev = dev->next) {
		if (dev->fd < 0) {
			continue;
		}
		FD_SET (dev->fd, fdset);
		if (dev->fd > *maxfd) {
			*maxfd = dev->fd;
		}
	}
}

void
inputlib_check_select (fd_set *fdset)
{
	inputlib_hotplug_check_select (fdset);

	for (device_t *dev = devices; dev; dev = dev->next) {
		if (dev->fd < 0) {
			continue;
		}
		if (FD_ISSET (dev->fd, fdset)) {
			read_device_input (dev);
		}
	}
}

int
inputlib_check_input (void)
{
	fd_set      fdset;
	struct timeval _timeout;
	struct timeval *timeout = &_timeout;
	int         res;
	int         maxfd = -1;

	_timeout.tv_sec = 0;
	_timeout.tv_usec = 0;

	FD_ZERO (&fdset);

	inputlib_add_select (&fdset, &maxfd);
	if (maxfd < 0) {
		return 0;
	}
	res = select (maxfd + 1, &fdset, NULL, NULL, timeout);
	if (res <= 0) {
		return 0;
	}

	inputlib_check_select (&fdset);
	return 1;
}

static void
close_device (device_t *dev)
{
	if (dev->next) {
		dev->next->prev = dev->prev;
	}
	*dev->prev = dev->next;

	if (device_remove) {
		device_remove (dev);
	}
	close (dev->fd);
	free (dev->button_map);
	if (dev->buttons) {
		free (dev->buttons);
	}
	free (dev->abs_axis_map);
	free (dev->rel_axis_map);
	if (dev->axes) {
		free (dev->axes);
	}
	free (dev->phys);
	free (dev->uniq);
	free (dev->name);
	free (dev->path);
	free (dev);
}

static char *
make_devname (const char *path, const char *name)
{
	int         plen = strlen (path);
	int         nlen = strlen (name);
	char       *devname = malloc (plen + nlen + 2);

	strcpy (devname, path);
	devname[plen] = '/';
	strcpy (devname + plen + 1, name);

	return devname;
}

static int
check_input_device (const char *path, const char *name)
{
	int         ret;
	char       *devname = make_devname (path, name);

	//puts (devname);
	ret = check_device (devname);
	free (devname);
	return ret;
}

static void
device_created (const char *name)
{
	char       *devname = make_devname (devinput_path, name);
	device_t   *dev;
	int         olddev = 0;

	for (dev = devices; dev; dev = dev->next) {
		if (strcmp (dev->path, devname) == 0) {
			// already have this device open
			olddev = 1;
			break;
		}
	}
	if (!olddev && check_device (devname) >= 0) {
		//Sys_Printf ("found device %s\n", devname);
	}
	free (devname);
}

static void
device_deleted (const char *name)
{
	char       *devname = make_devname (devinput_path, name);
	device_t  **dev;

	for (dev = &devices; *dev; dev = &(*dev)->next) {
		if (strcmp ((*dev)->path, devname) == 0) {
			//Sys_Printf ("lost device %s\n", (*dev)->path);
			close_device (*dev);
			break;
		}
	}
	free (devname);
}

static int
scan_devices (void)
{
	struct dirent *dirent;
	DIR *dir;

	dir = opendir (devinput_path);
	if (!dir) {
		return -1;
	}

	while ((dirent = readdir (dir))) {
		if (dirent->d_type != DT_CHR) {
			continue;
		}
		if (strncmp (dirent->d_name, "event", 5)) {
			continue;
		}
		if (check_input_device (devinput_path, dirent->d_name) < 0) {
			continue;
		}
		//Sys_Printf("%s\n", dirent->d_name);
	}
	closedir (dir);
	return 0;
}

int
inputlib_init (void (*dev_add) (device_t *), void (*dev_rem) (device_t *))
{
	device_add = dev_add;
	device_remove = dev_rem;
	if (scan_devices () != -1) {
		inputlib_hotplug_init (devinput_path, device_created, device_deleted);
		return 0;
	}
	return -1;
}

void
inputlib_close (void)
{
	inputlib_hotplug_close ();
	while (devices) {
		close_device (devices);
	}
}
