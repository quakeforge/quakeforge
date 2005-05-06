@extern void (string str) print = #0;
@extern integer () errno = #0;
@extern string (integer err) strerror = #0;
@extern integer (...) open = #0; // string path, float flags[, float mode]
@extern integer (integer handle) close = #0;
@extern string (integer handle, integer count, integer []result) read = #0;
@extern integer (integer handle, string buffer, integer count) write = #0;
@extern integer (integer handle, integer pos, integer whence) seek = #0;

@extern void() traceon = #0;          // turns statment trace on
@extern void() traceoff = #0;

@extern void (...) printf = #0;
