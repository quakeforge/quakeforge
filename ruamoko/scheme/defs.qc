void (string str) print = #0;
integer () errno = #0;
string (integer err) strerror = #0;
integer (...) open = #0; // string path, float flags[, float mode]
integer (integer handle) close = #0;
string (integer handle, integer count, integer []result) read = #0;
integer (integer handle, string buffer, integer count) write = #0;
integer (integer handle, integer pos, integer whence) seek = #0;

void() traceon = #0;          // turns statment trace on
void() traceoff = #0;

void (...) printf = #0;


float time;
entity self;

.float nextthink;
.void() think;
.float frame;
.vector origin;
