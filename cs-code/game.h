#ifndef __game_h
#define __game_h

@extern float(string s) cvar;		// return cvar.value
@extern void(string var, string val) cvar_set;    // sets cvar.value
@extern string(float f) ftos;		// converts float to string
@extern integer(float f) ftoi;		// converts float to integer
@extern string(integer i) itos;		// converts interger to string
@extern float(integer i) itof;		// converts interger to string
@extern integer(string str) stoi;	// converts string to integer
@extern float(string str) stof;		// converts string to float
@extern void(string s) dprint;

@extern entity self;
@extern .float nextthink;
@extern .float frame;
@extern .void () think;
@extern float time;

#endif//__game_h
