@extern void (string str) print;
@extern int () errno;
@extern string (int err) strerror;
@extern int (...) open; // string path, float flags[, float mode]
@extern int (int handle) close;
@extern string read (int handle, int count, int *result);
@extern int (int handle, string buffer, int count) write;
@extern int (int handle, int pos, int whence) seek;

@extern void() traceon;          // turns statment trace on
@extern void() traceoff;

@extern void (...) printf;
