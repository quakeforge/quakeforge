void (string str) print = #1;
void () garbage_collect = #2;
integer () errno = #3;
string (integer err) strerror = #4;
integer (...) open = #5; // string path, float flags[, float mode]
integer (integer handle) close = #6;
string (integer handle, integer count) read = #7; // FIXME: cf read_result
integer (integer handle, string buffer, integer count) write = #8;
integer (integer handle, integer pos, integer whence) seek = #9;

void() traceon = #10;          // turns statment trace on
void() traceoff = #11;

void (...) printf = #12;


float time;
entity self;
integer read_result;		// FIXME: hacky (cf read)

.float nextthink;
.void() think;
.float frame;
.vector origin;
