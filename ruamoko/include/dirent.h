#ifndef __ruamoko_dirent_h
#define __ruamoko_dirent_h

typedef @handle _dirent_t DIR;

typedef struct dirent {
	string name;
	int type;
	int errno;
} dirent_t;

DIR opendir (string path);
void closedir (DIR dir);
dirent_t readdir (DIR dir);
bool isdir (string path);

#endif//__ruamoko_dirent_h
