/*
 * DIRENT.H (formerly DIRLIB.H)
 *
 * by M. J. Weinstein   Released to public domain 1-Jan-89
 *
 * Because I have heard that this feature (opendir, readdir, closedir)
 * it so useful for programmers coming from UNIX or attempting to port
 * UNIX code, and because it is reasonably light weight, I have included
 * it in the Mingw32 package. I have also added an implementation of
 * rewinddir, seekdir and telldir.
 *   - Colin Peters <colin@bird.fu.is.saga-u.ac.jp>
 *
 *  This code is distributed in the hope that is will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAIMED. This includeds but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Revision$
 * $Author$
 * $Date$
 *
 */

#ifndef	__STRICT_ANSI__

#ifndef _DIRENT_H_
#define _DIRENT_H_

#include <io.h>
#include <stdio.h> /* for FILENAME_MAX in VC2005 (phrosty) */

#ifndef RC_INVOKED

#ifdef __cplusplus
extern      "C" {
#endif

       struct dirent {
	      long        d_ino;	/* Always zero. */
	      unsigned short d_reclen;	/* Always zero. */
	      unsigned short d_namlen;	/* Length of name in d_name. */
	      char        d_name[FILENAME_MAX];	/* File name. */
		  unsigned char  d_type;
       };

	   enum {
		   DT_UNKNOWN = 0,
		   DT_FIFO = 1,
		   DT_CHR = 2,
		   DT_DIR = 4,
		   DT_BLK = 6,
		   DT_REG = 8,
		   DT_LNK = 10,
		   DT_SOCK = 12,
		};
/*
 * This is an internal data structure. Good programmers will not use it
 * except as an argument to one of the functions below.
 * dd_stat field is now int (was short in older versions).
 */
       typedef struct {
	      /* disk transfer area for this dir */
	      struct _finddata_t dd_dta;

	      /* dirent struct to return from dir (NOTE: this makes this thread
	         safe as long as only one thread uses a particular DIR struct
	         at a time) */
	      struct dirent dd_dir;

	      /* _findnext handle */
	      intptr_t    dd_handle;

	      /*
	       * Status of search:
	       *   0 = not started yet (next entry to read is first entry)
	       *  -1 = off the end
	       *   positive = 0 based index of next entry
	       */
	      int         dd_stat;

	      /* given path for dir with search pattern (struct is extended) */
		  int         dd_namlen;
	      char        dd_name[1];
       } DIR;

       DIR        *qf_opendir (const char *);
       struct dirent * qf_readdir (DIR *);
       int qf_closedir (DIR *);
       void qf_rewinddir (DIR *);
       long qf_telldir (DIR *);
       void qf_seekdir (DIR *, long);

#define opendir qf_opendir
#define readdir qf_readdir
#define closedir qf_closedir
#define rewinddir qf_rewinddir
#define telldir qf_telldir
#define seekdir qf_seekdir


#ifdef	__cplusplus
}
#endif
#endif					/* Not RC_INVOKED */
#endif					/* Not _DIRENT_H_ */
#endif					/* Not __STRICT_ANSI__ */
