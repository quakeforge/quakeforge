# Quick-and-dirty Makefile for gas2masm

all:	gas2masm
gas2masm:	gas2masm.c
	cc $(CFLAGS) -Dstrcmpi=strcasecmp -Dstrnicmp=strncasecmp -o gas2masm gas2masm.c
