void (string str) print = #0;
int () errno = #0;
string (int err) strerror = #0;
int (...) open = #0; // string path, float flags[, float mode]
int (int handle) close = #0;
string read (int handle, int count, int *result) = #0;
int (int handle, string buffer, int count) write = #0;
int (int handle, int pos, int whence) seek = #0;

void() traceon = #0;          // turns statment trace on
void() traceoff = #0;

void (...) printf = #0;


float time;
entity self;

.float nextthink;
.void() think;
.float frame;
.vector origin;
