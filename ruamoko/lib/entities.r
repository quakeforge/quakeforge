#include "entities.h"

void (entity e, vector o) setorigin = #2;
void (entity e, string m) setmodel = #3;
void (entity e, vector min, vector max) setsize = #4;
entity () spawn = #14;
void (entity e) remove = #15;
entity (entity start, .string fld, string match) find = #18;
entity (vector org, float rad) findradius = #22;
entity (entity e) nextent = #47;
void (entity e) makestatic = #69;
void (entity e) setspawnparms = #78;
