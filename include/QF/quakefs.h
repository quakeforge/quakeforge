/*
	quakefs.h

	quake virtual filesystem definitions

	Copyright (C) 1996-1997  Id Software, Inc.
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

	$Id$
*/

#ifndef __quakefs_h
#define __quakefs_h

/** \defgroup quakefs Quake Filesystem
	\ingroup utils
*/
//@{

#include "QF/qtypes.h"
#include "QF/quakeio.h"

//============================================================================

/**	Simple list for file searches.
*/
typedef struct filelist_s {
	char      **list;			///< the actual list of files
	int         count;			///< the number of files in the list
	int         size;			///< the number of files that can be in the
								///< list before reallocation is required.
} filelist_t;

/**	Cached information about the current game directory. \see \ref dirconf.
*/
typedef struct gamedir_s {
	const char *name;			///< the tag name built from game and gamedir
	const char *gamedir;		///< the actual game dir
	const char *path;			///< colon separated list of search paths
	const char *gamecode;		///< name of file to load for gamecode
	struct {
		const char *def;		///< directory to which to write other files
		const char *skins;		///< directory to which to write skins
		const char *models;		///< directory to which to write models
		const char *sound;		///< directory to which to write sounds
		const char *maps;		///< directory to which to write maps
	} dir;
} gamedir_t;

/**	Cached information about the current game directory. \see \ref dirconf.
*/
extern gamedir_t *qfs_gamedir;

/** Function type of callback called on gamedir change.
	\param phase	0 = before Cache_Flush(), 1 = after Cache_Flush()
*/
typedef void gamedir_callback_t (int phase);

/**	Base of the QFS user directory tree. The QFS functions, except for
	QFS_FOpenFIle() and _QFS_FOpenFile(),  will never access a file outside of
	this tree. Set via the \c fs_userpath directory.
*/
extern const char *qfs_userpath;

/**	Indicates the found file came from a pak file.

	Set by QFS_FOpenFIle() and _QFS_FOpenFile().
*/
extern int file_from_pak;

/**	The size of the file found via QFS_FOpenFIle() or _QFS_FOpenFile().

	Set by QFS_FOpenFIle() and _QFS_FOpenFile().
*/
extern int qfs_filesize;

struct cache_user_s;
struct dstring_s;

/**	Initialize the Quake Filesystem.

	This function initializes the \c fs_sharepath, \c fs_userpath and
	\c fs_dirconf Cvars. It then loads the \ref dirconf and parses the
	\c -game command line option.

	\param game		The game type used for searching the directory
					configuration. Currently, this is \"qw\" for
					quakeworld clients and servers, and one of \"nq\",
					\"hexen\", \"rogue\" or \"abyss\" for the netquake
					clients and servers.
*/
void QFS_Init (const char *game);

/** Change the current game directory.

	The configuration for the game directory is taken from \ref dirconf.
	Sets the fields in ::qfs_gamedir. Can be called at any time (and is, by
	the quakeworld clients and servers).

	\param gamedir	The directory to which to set the game directory.
*/
void QFS_Gamedir (const char *gamedir);

/** Open a file from within the user directory.

	If \a path attempts to leave the user directory, this function sets
	errno to EACCESS and returns NULL. If \a mode indicates a write
	operation, then this function will make sure that any directories
	leading to the file exist.

	\param path		Path name of file to open, relative to ::qfs_userpath.
	\param mode		Mode in which to open the file. Passed through as-is to
					Qopen ().
	\return			The file handle as returned by Qopen or NULL on error.

	\note This function cannot be used to access files within a pak file.
	For pak file support, use QFS_FOpenFile().
*/
QFile *QFS_Open (const char *path, const char *mode);

/** Open a file for writing.

	This is a convenience wrapper for QFS_Open(). If \a zip is true, then
	the file will be written uzing gzip compression if QuakeForge was built
	with zlib support, otherwise the file will be written uncompressed.

	\param path		Path name of file to open, relative to ::qfs_userpath.
	\param zip		Compression control.
	\return			The file handle as returned by Qopen or NULL on error.
*/
QFile *QFS_WOpen (const char *path, int zip);

/** Write a block of data to a file.

	This is a convenience wrapper for QFS_Open(). 

	\param filename	Path name of file to open, relative to ::qfs_userpath.
	\param data		Pointer to the block of data to be written.
	\param len		The size of the block of data, in bytes.

	\note If an error occurs, this function will call Sys_Error(), aborting
	the program.
*/
void QFS_WriteFile (const char *filename, const void *data, int len);

/**	Open a file for reading.

	The file will be searched for through all the subdirectories given in the
	\c Path attribute (\ref dirconf). For each directory in \c Path, this
	function will check \c \${fs_userpath}/${dir} then
	\${fs_sharepath}/${dir}. In each location, pak files are checked before
	external files. pak files will be searched in reverse order (pak10.pak,
	pak9.pak, ... pak2.pak,pak1.pak,pak0.pak). However, any file ending in
	.pak will be checked whether it is a pak file.

	Does name mangling for files with .gz extention and .wav-.ogg
	substitution.

	\param filename	The name of the file to open.
	\param gzfile	Address of file handle pointer.
	\param foundname If not NULL, will be set to the real name of the file.
	\param zip		If true and the file has been compressed with gzip, the
					file will be opened such that it decompresses on the fly.
					Otherwise, the file will be read as-is.
	\return			The amount of bytes that can be read from the file handle.
					This will be either the files true size if \a zip is true,
					or the compressed size of \a zip is false. If an error
					occurs while opening the file, this will be -1 and
					\a *gzfile will be set to NULL.
*/
int _QFS_FOpenFile (const char *filename, QFile **gzfile,
					struct dstring_s *foundname, int zip);

/**	Open a file for reading.

	This is a convenience wrapper for _QFS_FOpenFile(). The file will
	always be opened with decompression enabled. See the documentation for
	_QFS_FOpenFile() for more details.

	\param filename	The name of the file to open.
	\param gzfile	Address of file handle pointer.
	\return			The amount of bytes that can be read from the file handle.
					If an error occurs while opening the file, this will be
					-1 and \a *gzfile will be set to NULL.
*/
int QFS_FOpenFile (const char *filename, QFile **gzfile);

/**	Load a file into memory.

	This is a convenience wrapper for QFS_FOpenFile(). The file will be loaded
	in memory allocated from the location inicated by usehunk.

	\param path		The name of the file to load.
	\param usehunk	The location from which to allocate memory for the file's
					data. Use 0.
	\return			Pointer to the file's data, or NULL on error.
	\todo remove \a usehunk
*/
byte *QFS_LoadFile (const char *path, int usehunk);

/** Load a file into memeory.

	This is a wrapper for QFS_LoadFile().

	\deprecated This should go away soon.
*/
byte *QFS_LoadStackFile (const char *path, void *buffer, int bufsize);

/** Load a file into memeory.

	The file is loaded into memory allocated from the hunk.
*/
byte *QFS_LoadHunkFile (const char *path);

/** Load a file into memeory.

	This is a wrapper for QFS_LoadFile().

	\deprecated This should go away soon.
*/
void QFS_LoadCacheFile (const char *path, struct cache_user_s *cu);

/**	Rename a file.

	\param old_path	The file to rename.
	\param new_path	The new name for the file.
	\return			0 for success, -1 for failure.

	\note The file and destination must both be within \c qfs_userpath.
*/
int QFS_Rename (const char *old_path, const char *new_path);

/** Delete a file.

	\param path		The file to delete.
	\return			0 for success, -1 for failure.

	\note The file must be within \c qfs_userpath.
*/
int QFS_Remove (const char *path);

/**	Find available filename.

	The filename will be of the form \c prefixXXXX.ext where \c XXXX
	is a zero padded number from 0 to 9999.

	\param filename	This will be set to the available filename.
	\param prefix	The part of the filename preceeding the numers.
	\param ext		The extension to add to the filename.
	\return			1 for success, 0 for failure.

	\note \a prefix is relative to \c qfc_userpath.
*/
int QFS_NextFilename (struct dstring_s *filename, const char *prefix,
					  const char *ext);

/** Extract the non-extension part of the file name from the path.

	\param in		The path from which to extract the name.
	\return			The extracted name.
	\note It is the caller's responsibility to free the extracted name.
*/
char *QFS_FileBase (const char *in);

/**	Set the file extention if not already present.

	If the file already has an extension, do nothing.

	\param path		The path to which the extension will be added.
	\param extension The extension to add.
*/
void QFS_DefaultExtension (struct dstring_s *path, const char *extension);

/**	Set the file extention.

	If the file already has an extension, it will be replaced.

	\param path		The path to which the extension will be set.
	\param extension The extension to set.
*/
void QFS_SetExtension (struct dstring_s *path, const char *extension);

/**	Remove any extension from the path.

	\param in		The path from which to strip the extension.
	\param out		The destination of the stripped path. May be the same
					as \a in.
*/
void QFS_StripExtension (const char *in, char *out);

/** Create a canonical path.

	Convert all \\ to /, remove all . elements from the path and resolve
	all foo/..

	\param pth		The path to make canonical
	\return			The canonical path.
	\note			It is the caller's responsibility to free the canonical
					path.
*/
char *QFS_CompressPath (const char *pth);

/**	Return a pointer to the start of the filename part of the path.

	\param pathname	The path withing which to find the filename.
	\return			Pointer to the beginning of the filename. This points
					inside \a pathname.
*/
const char *QFS_SkipPath (const char *pathname);

/**	Return a pointer to the start of the extention part of the path.

	\param in		The path withing which to find the extention.
	\return			Pointer to the beginning of the extention. This points
					inside \a in.
*/
const char *QFS_FileExtension (const char *in);

/**	Register a callback function for when the gamedir changes.

	The callbacks will be called in order of registration, in two passes.
	The first pass (\c phase is 0) is before the cache has been flushed.
	The second pass (\c phase is 1) is after the cache has been flushed.

	\param func		The function to call every time the gamedir changes via
					QFS_Gamedir().
*/
void QFS_GamedirCallback (gamedir_callback_t *);

/**	Create a new file list.

	\return			Pointer to the new file list.
*/
filelist_t *QFS_FilelistNew (void);

/**	Add a single file to the file list.

	If the extension of the file matches \a ext, it will be trimmed off the
	file name befor the file is added to the list.

	\param filelist	The list to which the file is to be added.
	\param fname	The file which is to be added to the file list.
	\param ext		The extention to be stripped from the file.
	\todo Should the extension stripping be done by the caller?
*/
void QFS_FilelistAdd (filelist_t *filelist, const char *fname,
					  const char *ext);
/**	Add files from the gamedir within the specified path.

	All files from the specified path in the gamedir with the given extension
	will be added to the list after possibly stripping their extension.

	\param list		The list to which the files are to be added.
	\param path		The path withing the gamedir from which files are to be
					added to the list.
	\param ext		The extension of the files to be added.
	\param strip	If true, the extension of the files will be stripped.
*/
void QFS_FilelistFill (filelist_t *list, const char *path, const char *ext,
					   int strip);

/**	Free a file list.

	\param list		The list to be freed.
*/
void QFS_FilelistFree (filelist_t *list);


/**	Expand leading "~/" in \a path to the user's home directory.
	On Linux-like systems, the user's home directory is obtained from the
	system, or failing that, the \c HOME environment variable.

	On Windows systems, first the \c HOME environment variable is checked.
	If \c HOME is not set, \c WINDIR is used.

	\param path		the path to check for "~/"
	\return			the expanded path
	\note It is the caller's responsibility to free the returned string.
	FIXME: rename to QFS_*
*/
char *expand_squiggle (const char *path);

//@}

#endif // __quakefs_h
