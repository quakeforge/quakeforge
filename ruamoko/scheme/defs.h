@extern void (string str) print = #0;
@extern int () errno = #0;
@extern string (int err) strerror = #0;
@extern int (...) open = #0; // string path, float flags[, float mode]
@extern int (int handle) close = #0;
@extern string read (int handle, int count, int *result) = #0;
@extern int (int handle, string buffer, int count) write = #0;
@extern int (int handle, int pos, int whence) seek = #0;

@extern void() traceon = #0;          // turns statment trace on
@extern void() traceoff = #0;

@extern void (...) printf = #0;
