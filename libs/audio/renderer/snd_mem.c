/*
	snd_mem.c

	sound memory management

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2003 Bill Currie <bill@taniwha.org>

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "compat.h"
#include "snd_internal.h"

static uint32_t snd_mem_size;
static cvar_t snd_mem_size_cvar = {
	.name = "snd_mem_size",
	.description =
		"Amount of LOCKED memory to allocate to the sound system in MB. "
		"Defaults to 32MB.",
	.default_value = "64",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_uint, .value = &snd_mem_size },
};

static memzone_t *snd_zone;

void
SND_Memory_Init_Cvars (void)
{
	qfZoneScoped (true);
	Cvar_Register (&snd_mem_size_cvar, nullptr, nullptr);
}

static void __attribute__((format(PRINTF,2,3)))
snd_zone_error (void *data, const char *fmt, ...)
{
	qfZoneScoped (true);
	va_list     args;
	dstring_t  *msg = dstring_new ();

	va_start (args, fmt);
	dvsprintf (msg, fmt, args);
	Sys_Error ("Sound: %s", msg->str);
}

static void
snd_memory_shutdown (void *data)
{
	qfZoneScoped (true);
	if (snd_zone) {
		size_t      size = snd_mem_size * 1024 * 1024;
		Sys_Free (snd_zone, size);
	}
}

bool
SND_Memory_Init (void)
{
	qfZoneScoped (true);
	size_t      size = snd_mem_size * 1024 * 1024;

	snd_zone = Sys_Alloc (size);
	if (!snd_zone) {
		Sys_Printf (RED"Sound: Unable to allocate %uMB buffer"DFL"\n",
					snd_mem_size);
		return false;
	}
	if (!Sys_LockMemory (snd_zone, size)) {
		Sys_Printf (RED"Sound: Unable to lock %uMB buffer"DFL"\n",
					snd_mem_size);
		//FIXME Permission issue?
		//Sys_Free (snd_zone, size);
		//return false;
	}
	Z_ClearZone (snd_zone, size, 0, 1);
	Z_SetError (snd_zone, snd_zone_error, nullptr);

	Sys_MaskPrintf (SYS_snd, "Sound: Initialized %uMB buffer\n", snd_mem_size);

	Sys_RegisterShutdown (snd_memory_shutdown, nullptr);
	return true;
}

sfxbuffer_t *
SND_Memory_AllocBuffer (unsigned samples)
{
	qfZoneScoped (true);
	size_t      size = offsetof (sfxbuffer_t, data[samples]);
	// Z_Malloc (currently) clears memory, don't need that for the whole
	// buffer (just the header), but Z_TagMalloc does not
	// +4 for sentinel
	sfxbuffer_t *buffer = Z_TagMalloc (snd_zone, size + 4, 1);
	if (buffer) {
		// clear buffer header
		memset (buffer, 0, sizeof (sfxbuffer_t));
		// place a sentinel at the end of the buffer for added safety
		memcpy (&buffer->data[samples], "\xde\xad\xbe\xef", 4);
	} else {
		Sys_Printf (RED"Sound: out of memory: %uMB exhausted"DFL"\n",
					snd_mem_size);
	}
	return buffer;
}

void
SND_Memory_Free (void *ptr)
{
	qfZoneScoped (true);
	Z_Free (snd_zone, ptr);
}

void
SND_Memory_SetTag (void *ptr, int tag)
{
	qfZoneScoped (true);
	Z_SetTag (snd_zone, ptr, tag);
}

int
SND_Memory_Retain (void *ptr)
{
	qfZoneScoped (true);
	return Z_IncRetainCount (snd_zone, ptr);
}

int
SND_Memory_Release (void *ptr)
{
	qfZoneScoped (true);
	int         retain =  Z_DecRetainCount (snd_zone, ptr);
	if (!retain) {
		Z_Free (snd_zone, ptr);
	}
	return retain;
}

int
SND_Memory_GetRetainCount (void *ptr)
{
	qfZoneScoped (true);
	return Z_GetRetainCount (snd_zone, ptr);
}

static sfxbuffer_t *
snd_open_fail (sfx_t *sfx)
{
	qfZoneScoped (true);
	return nullptr;
}

bool
SND_Load (sfx_t *sfx)
{
	qfZoneScoped (true);
	char       *realname;
	char        buf[4];
	QFile      *file;

	sfx->open = snd_open_fail;

	file = QFS_FOpenFile (sfx->name);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", sfx->name);
		return false;
	}
	if (!strequal (qfs_foundfile.realname, sfx->name)) {
		realname = strdup (qfs_foundfile.realname);
	} else {
		realname = (char *) sfx->name;	// won't free if realname == sfx->name
	}
	Qread (file, buf, 4);
	Qseek (file, 0, SEEK_SET);
#ifdef HAVE_VORBIS
	if (strnequal ("OggS", buf, 4)) {
		Sys_MaskPrintf (SYS_snd, "SND_Load: ogg file\n");
		if (!SND_LoadOgg (file, sfx, realname))
			goto bail;
		return true;
	}
#endif
#ifdef HAVE_FLAC
	if (strnequal ("fLaC", buf, 4)) {
		Sys_MaskPrintf (SYS_snd, "SND_Load: flac file\n");
		if (!SND_LoadFLAC (file, sfx, realname))
			goto bail;
		return true;
	}
#endif
#ifdef HAVE_WILDMIDI
	if (strnequal ("MThd", buf, 4)) {
		Sys_MaskPrintf (SYS_snd, "SND_Load: midi file\n");
		if (!SND_LoadMidi (file, sfx, realname))
			goto bail;
		return true;
	}
#endif
	if (strnequal ("RIFF", buf, 4)) {
		Sys_MaskPrintf (SYS_snd, "SND_Load: wav file\n");
		if (!SND_LoadWav (file, sfx, realname))
			goto bail;
		return true;
	}
bail:
	Qclose (file);
	if (realname != sfx->name)
		free (realname);
	return false;
}
