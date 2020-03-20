#include <entities.h>

void setmodel (entity e, string m) = #3;
void setorigin (entity e, vector o) = #2;
void setsize (entity e, vector min, vector max) = #4;
entity spawn (void) = #14;
void remove (entity e) = #15;
#ifdef __VERSION6__
entity find (entity start, .string field, string match) = #18;
#else
entity find (entity start, ...) = #18;
#endif
entity findradius (vector origin, float radius) = #22;
entity nextent (entity e) = #47;
void makestatic (entity e) = #69;
void setspawnparms (entity e) = #78;

void EntityParseFunction (void func (string ent_data)) = #0;
