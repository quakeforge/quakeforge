/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2007 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifdef HAVE_LIBCURL

#include <curl/curl.h>

#include "QF/dstring.h"
#include "QF/sys.h"

#include "qw/include/cl_http.h"
#include "qw/include/cl_parse.h"
#include "qw/include/client.h"

static int curl_borked;
static CURL *easy_handle;
static CURLM *multi_handle;

static int
http_progress (void *clientp, double dltotal, double dlnow,
			   double ultotal, double uplow)
{
	return 0;	//non-zero = abort
}

static size_t
http_write (void *ptr, size_t size, size_t nmemb, void *stream)
{
	if (!cls.download) {
		Sys_Printf ("http_write: unexpected call\n");
		return -1;
	}
	return Qwrite (cls.download, ptr, size * nmemb);
}

void
CL_HTTP_Init (void)
{
	if ((curl_borked = curl_global_init (CURL_GLOBAL_NOTHING)))
		return;
	multi_handle = curl_multi_init ();
}

void
CL_HTTP_Shutdown (void)
{
	if (curl_borked)
		return;
	curl_multi_cleanup (multi_handle);
	curl_global_cleanup ();
}

void
CL_HTTP_StartDownload (void)
{
	easy_handle = curl_easy_init ();

	curl_easy_setopt (easy_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt (easy_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt (easy_handle, CURLOPT_PROGRESSFUNCTION, http_progress);
	curl_easy_setopt (easy_handle, CURLOPT_WRITEFUNCTION, http_write);

	curl_easy_setopt (easy_handle, CURLOPT_URL, cls.downloadurl->str);
	curl_multi_add_handle (multi_handle, easy_handle);
}

void
CL_HTTP_Update (void)
{
	int         running_handles;
	int         messages_in_queue;
	CURLMsg    *msg;

	curl_multi_perform (multi_handle, &running_handles);
	while ((msg = curl_multi_info_read (multi_handle, &messages_in_queue))) {
		if (msg->msg == CURLMSG_DONE) {
			long        response_code;

			curl_easy_getinfo (msg->easy_handle, CURLINFO_RESPONSE_CODE,
							   &response_code);
			if (response_code == 200) {
				CL_FinishDownload ();
			} else {
				Sys_Printf ("download failed: %ld\n", response_code);
				CL_FailDownload ();
			}
			CL_HTTP_Reset ();
		}
	}
}

void
CL_HTTP_Reset (void)
{
	curl_multi_remove_handle (multi_handle, easy_handle);
	curl_easy_cleanup (easy_handle);
	easy_handle = 0;
}

#else

#include "qw/include/cl_http.h"

void CL_HTTP_Init (void) {}
void CL_HTTP_Shutdown (void) {}
void CL_HTTP_StartDownload (void) {}
void CL_HTTP_Update (void) {}
void CL_HTTP_Reset (void) {}

#endif
