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

*/

#ifndef __QF_quakefs_h
#define __QF_quakefs_h

/** \defgroup quakefs Quake Filesystem
	\ingroup utils
*/
///@{

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
	const char *hudtype;		///< name of the hud type
	struct {
		const char *def;		///< directory to which to write other files
		const char *skins;		///< directory to which to write skins
		const char *models;		///< directory to which to write models
		const char *sound;		///< directory to which to write sounds
		const char *maps;		///< directory to which to write maps
		const char *shots;		///< directory to which to write screenshots
	} dir;
} gamedir_t;

typedef struct vpath_s vpath_t;

typedef struct findfile_s {
	const vpath_t *vpath;		///< vpath in which file was found
	const char *realname;		///< the name of the file as found (may have
								///< .gz appended, or .ogg substituded for
								///< .wav) does not include the path
	bool        in_pak;			///< if true, path refers to a pak file rather
								///< than a directory
} findfile_t;

/**	Cached information about the current game directory. \see \ref dirconf.
*/
extern gamedir_t *qfs_gamedir;

/** Function type of callback called on gamedir change.
	\param phase	0 = before Cache_Flush(), 1 = after Cache_Flush()
	\param data		data pointer passed on to the callback
*/
typedef void gamedir_callback_t (int phase, void *data);

/**	Base of the QFS user directory tree. The QFS functions, except for
	QFS_FOpenFile() and _QFS_FOpenFile(),  will never access a file outside of
	this tree. Set via the \c fs_userpath directory.
*/
extern const char *qfs_userpath;

/**	Gives information about the last file opened by the FOpenFile functions.

	Set by QFS_FOpenFile() and _QFS_FOpenFile().
*/
extern findfile_t qfs_foundfile;

/**	The size of the file found via QFS_FOpenFile() or _QFS_FOpenFile().

	Set by QFS_FOpenFile() and _QFS_FOpenFile().
*/
extern int qfs_filesize;

struct cache_user_s;
struct dstring_s;
struct memhunk_s;
struct plitem_s;

/**	Initialize the Quake Filesystem.

	This function initializes the \c fs_sharepath, \c fs_userpath and
	\c fs_dirconf Cvars. It then loads the \ref dirconf and parses the
	\c -game command line option.

	\param hunk		Memory pool to use for hunk-based allocations.
	\param game		The game type used for searching the directory
					configuration. Currently, this is \"qw\" for
					quakeworld clients and servers, and one of \"nq\",
					\"hexen\", \"rogue\" or \"abyss\" for the netquake
					clients and servers.
*/
void QFS_Init (struct memhunk_s *hunk, const char *game);

/**	Set the directory configuration prior to initialization

	This allows the default configuration to be overriden at build time. Must
	be called before QFS_Init is called. fs_dirconf will still allow the
	configuration to be overridden.

	\param config	Property list specifity the directory layout used by the
					engine. The list will be 'retained'.
*/
void QFS_SetConfig (struct plitem_s *config);

/** Change the current game directory.

	The configuration for the game directory is taken from \ref dirconf.
	Sets the fields in ::qfs_gamedir. Can be called at any time (and is, by
	the quakeworld clients and servers).

	\param gamedir	The directory to which the game directory will be set.
*/
void QFS_Gamedir (const char *gamedir);

/** Search for a file in the quake file-system.

	The search will begin in the \a start vpath and end in the \a end vpath.
	If \a start is null, the search will begin in the vpath specified by
	qfs_vpaths (ie, the first directory in the \c Path attribute
	(\ref dirconf)). If \a end is null, the search will continue to the end
	of the list of vpaths. If \a start and \a end are the same (and non-null),
	then only the one vpath will be searched.

	\note	All search paths in a vpath will be searched. This keeps \QF's
			directory system transparent.

	\note	It is a fatal error for \a end to be non-null but not in the list
			of vpaths.

	\warning	The returned pointer is to a static instance of findfile_t and
				thus will be overwritten on the next call to any of the search
				functions (QFS_FindFile, QFS_FOpenFile, _QFS_FOpenFile)

	\param fname	The name of the file to be searched for.
	\param start	The first vpath (gamedir) to search.
	\param end		The last vpath (gamedir) to search.
	\return			Pointer to the findfile_t record indicating the location
					of the file, or null if the file was not found.
*/
findfile_t *QFS_FindFile (const char *fname, const vpath_t *start,
						  const vpath_t *end);

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

QFile *_QFS_VOpenFile (const char *filename, int zip,
					   const vpath_t *start, const vpath_t *end);
QFile *QFS_VOpenFile (const char *filename,
					  const vpath_t *start, const vpath_t *end);

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
	\param zip		If true and the file has been compressed with gzip, the
					file will be opened such that it decompresses on the fly.
					Otherwise, the file will be read as-is.
	\return			The file handle or NULL if there is an error.
*/
QFile *_QFS_FOpenFile (const char *filename, int zip);

/**	Open a file for reading.

	This is a convenience wrapper for _QFS_FOpenFile(). The file will
	always be opened with decompression enabled. See the documentation for
	_QFS_FOpenFile() for more details.

	\param filename	The name of the file to open.
	\return			The file handle pointer, or NULL if there is an error.
*/
QFile *QFS_FOpenFile (const char *filename);

/**	Load a file into memory.

	The file will be loaded into memory allocated from the location indicated
	by \a usehunk.

	\param file		The handle of the file to load.
	\param usehunk	The location from which to allocate memory for the file's
					data. Use 0.
	\return			Pointer to the file's data, or NULL on error.
	\todo remove \a usehunk

	\note The file is closed on return as any error is either file not found
	(\a file is null) or there was a memory allocation error (bigger
	problems).
*/
byte *QFS_LoadFile (QFile *file, int usehunk);

/** Load a file into memeory.

	The file is loaded into memory allocated from the hunk.
*/
byte *QFS_LoadHunkFile (QFile *file);

/** Load a file into memeory.

	This is a wrapper for QFS_LoadFile().

	\deprecated This should go away soon.
*/
void QFS_LoadCacheFile (QFile *file, struct cache_user_s *cu);

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
	is a zero padded number from 0 to 9999. Should there already be 10000
	files of such a pattern, then extra digits will be added.

	\param filename	This will be set to the available filename.
	\param prefix	The part of the filename preceeding the numers.
	\param ext		The extension to add to the filename.
	\return			NULL for failure (with an error message in \a filename)
					or a quakeio file handle.

	\note \a prefix is relative to \c qfc_userpath, as is the generated
	filename.
*/
QFile *QFS_NextFile (struct dstring_s *filename, const char *prefix,
					 const char *ext);

/** Extract the non-extension part of the file name from the path.

	\param in		The path from which the name will be extracted.
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
	\note			No size checking is done on \a out. It use the
					caller's responsibility to ensure out is large enough
					to hold the stripped path. However, the stripped path
					will never be longer than the original path.
*/
void QFS_StripExtension (const char *in, char *out);

/** Create a canonical path.

	Convert all \c \\ to \c /, remove all \c . elements from the path and
	resolve all \c foo/.. pairs.

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
const char *QFS_SkipPath (const char *pathname) __attribute__((pure));

/**	Return a pointer to the start of the extention part of the path.

	\param in		The path withing which to find the extention.
	\return			Pointer to the beginning of the extention. This points
					inside \a in at all times. If the path has no extension,
					the returned pointer will point to the terminating nul
					of the path.
*/
const char *QFS_FileExtension (const char *in) __attribute__((pure));

/**	Register a callback function for when the gamedir changes.

	The callbacks will be called in order of registration, in two passes.
	The first pass (\c phase is 0) is before the cache has been flushed.
	The second pass (\c phase is 1) is after the cache has been flushed.

	\param func		The function to call every time the gamedir changes via
					QFS_Gamedir().
	\param data		Opaque data pointer passed to the callback.
*/
void QFS_GamedirCallback (gamedir_callback_t *func, void *data);

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
	\bug can escape the quake file system
*/
void QFS_FilelistFill (filelist_t *list, const char *path, const char *ext,
					   int strip);

/**	Free a file list.

	\param list		The list to be freed.
*/
void QFS_FilelistFree (filelist_t *list);

///@}

#endif//__QF_quakefs_h
