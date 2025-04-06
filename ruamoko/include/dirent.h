#ifndef __ruamoko_dirent_h
#define __ruamoko_dirent_h

typedef @handle _dirent_t DIR;

typedef struct dirinfo_s {
	string name;
	int type;
	int errno;
} dirinfo_t;

DIR opendir (string path);
void closedir (DIR dir);
dirinfo_t readdir (DIR dir);

#endif//__ruamoko_dirent_h
