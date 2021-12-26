#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evdev/hotplug.h"

static int inotify_fd;
static int devinput_wd;
static char *devinput_path;
static void (*device_deleted) (const char *name);
static void (*device_created) (const char *name);

static unsigned
get_queue_size (int fd)
{
	unsigned queue_len;
	int ret = ioctl (fd, FIONREAD, &queue_len);
	if (ret < 0) {
		perror ("ioctl");
		return 0;
	}
	return queue_len;
}

static void
parse_inotify_events (int fd)
{
	ssize_t len, i;
	ssize_t queue_len = get_queue_size (fd);
	char *buf = alloca (queue_len);
	struct inotify_event *event;

	len = read (fd, buf, queue_len);
	for (i = 0; i < len; i += sizeof (struct inotify_event) + event->len) {
		event = (struct inotify_event *) &buf[i];
		//printf ("%-3d %08x %5u %4d %s\n", event->wd, event->mask,
		//		event->cookie, event->len,
		//		event->len ? event->name : "<no name>");
		if ((event->mask & IN_DELETE) && event->len) {
			if (strncmp (event->name, "event", 5) == 0) {
				// interested in only evdev devices
				//printf("deleted device %s\n", event->name);
				device_deleted (event->name);
			}
		}
		if (((event->mask & IN_ATTRIB) || (event->mask & IN_CREATE))
			&& event->len) {
			// done this way because may not have read permission when the
			// device is created, so try again when (presumabely) permission is
			// granted
			if (strncmp (event->name, "event", 5) == 0) {
				// interested in only evdev devices
				//printf("created device %s\n", event->name);
				device_created (event->name);
			}
		}
	}
}

int
inputlib_hotplug_init(const char *path,
					  void (*created) (const char*),
					  void (*deleted) (const char *))
{
	inotify_fd = inotify_init ();
	if (inotify_fd == -1) {
		perror ("inotify_init");
		return -1;
	}
	devinput_wd = inotify_add_watch (inotify_fd, path,
									 IN_CREATE | IN_DELETE | IN_ATTRIB
									 | IN_ONLYDIR);
	if (devinput_wd == -1) {
		perror ("inotify_add_watch");
		close (inotify_fd);
		return -1;
	}
	devinput_path = strdup (path);
	device_created = created;
	device_deleted = deleted;
	//printf ("inputlib_hotplug_init: %s %d %d\n", path, inotify_fd,
	//		devinput_wd);
	return 0;
}

void
inputlib_hotplug_close (void)
{
	if (inotify_fd != -1) {
		close (inotify_fd);
		free (devinput_path);
		device_created = 0;
		device_deleted = 0;
	}
}

int
inputlib_hotplug_add_select (fd_set *fdset, int *maxfd)
{
	if (inotify_fd != -1) {
		FD_SET (inotify_fd, fdset);
		if (inotify_fd > *maxfd) {
			*maxfd = inotify_fd;
		}
	}
	return inotify_fd;
}

int
inputlib_hotplug_check_select (fd_set *fdset)
{
	if (inotify_fd != -1) {
		if (FD_ISSET (inotify_fd, fdset)) {
			parse_inotify_events (inotify_fd);
			return 1;
		}
	}
	return 0;
}
