/*
	vid_wl_sw.c

	Software Wayland video driver (8/32 bit)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#include <sys/mman.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/ui/view.h"

#include "context_wl.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

static struct wl_buffer *wl_buf;

static void
wl_set_palette (sw_ctx_t *ctx, const byte *palette)
{
}

static void
wl_sw8_8_update (sw_ctx_t *ctx, vrect_t *rects)
{
    if (wl_surface_configured) {
        wl_surface_attach (wl_surf, wl_buf, 0, 0);
        wl_surface_damage_buffer (wl_surf, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_commit (wl_surf);
    }

    struct timespec ts = {};
    wl_display_dispatch_timeout(wl_disp, &ts);
}

static sw_framebuffer_t swfb;
static framebuffer_t fb = { .buffer = &swfb };

static void
shm_name (char *buf)
{
    struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	auto r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A' + (r & 15) + (r & 16) * 2;
		r >>= 5;
	}
}

static int
create_shm_file (void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        shm_name(name + sizeof(name) - 7);
        
        retries--;

        auto fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }

    } while (retries > 0 && errno == EEXIST);

    return -1;
}

static int
allocate_shm_file (size_t size)
{
    auto fd = create_shm_file ();
    if (fd < 0)
        Sys_Error ("VID: create_shm_file failed");

    int ret;
    do {
        ret = ftruncate (fd, size);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0)
        Sys_Error ("VID: allocate_shm_file failed to truncate file");

    return fd;
}

static struct wl_buffer *
allocate_shm_buffer (int fb)
{
    auto stride = viddef.width * 4;
    auto offset = viddef.height * stride * fb;
    return wl_shm_pool_create_buffer(wl_shm_pool, offset, viddef.width, viddef.height,
                                    stride, WL_SHM_FORMAT_XRGB8888);
}

static void
wl_init_buffers (void *data)
{
    sw_ctx_t *ctx = data;

    ctx->framebuffer = &fb;

    fb.width = viddef.width;
    fb.height = viddef.height;

    auto stride = viddef.width * 4;
    auto pool_size = viddef.height * stride * 2;
    auto fd = allocate_shm_file(pool_size);
    uint8_t *pool_data = mmap (nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    wl_shm_pool = wl_shm_create_pool(wl_shm, fd, pool_size);

    wl_buf = allocate_shm_buffer(0);

    auto offset = viddef.height * stride * 0;
    auto pixels = (uint32_t*)&pool_data[offset];
    memset (pixels, 0, viddef.width * viddef.height * 4);

    swfb.color = pool_data;
    swfb.depth = nullptr;
    swfb.rowbytes = stride;

    if (wl_surface_configured) {
        wl_surface_attach (wl_surf, wl_buf, 0, 0);
        wl_surface_damage (wl_surf, 0, 0, UINT32_MAX, UINT32_MAX);
        wl_surface_commit (wl_surf);
    }
}

static void
wl_create_context (sw_ctx_t *ctx)
{
    viddef.vid_internal->init_buffers = wl_init_buffers;
}

static void
wl_choose_visual (sw_ctx_t *ctx)
{
}

sw_ctx_t *
WL_SW_Context (vid_internal_t *vi)
{
    sw_ctx_t *ctx = calloc (1, sizeof (sw_ctx_t));
	ctx->set_palette = wl_set_palette;
	ctx->choose_visual = wl_choose_visual;
	ctx->create_context = wl_create_context;
	ctx->update = wl_sw8_8_update;

	vi->ctx = ctx;
	return ctx;
}

void
WL_SW_Init_Cvars (void)
{
}
