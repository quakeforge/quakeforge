void (string str) print = #1;
void () garbage_collect = #2;
float () errno = #3;
string (float err) strerror = #4;
float (...) open = #5; // string path, float flags[, float mode]
float (float handle) close = #6;
string (float handle, float count) read = #7; // FIXME: cf read_result
float (float handle, string buffer, float count) write = #8;
float (float handle, float pos, float whence) seek = #9;

void() traceon = #10;          // turns statment trace on
void() traceoff = #11;


float time;
entity self;
float read_result;		// FIXME: hacky (cf read)

.float nextthink;
.void() think;
.float frame;
.vector origin;
