#ifndef __def_h
#define __def_h

#include <QF/pr_comp.h>

typedef struct {
	etype_t			type;
	unsigned short	offset;
	const char		*name;
} def_t;

extern def_t nq_global_defs[];
extern def_t nq_field_defs[];
extern int nq_crc;

extern def_t qw_global_defs[];
extern def_t qw_field_defs[];
extern int qw_crc;

void Init_Defs (def_t *gtab, def_t *ftab);
def_t *Find_Global_Def_name (const char *name);
def_t *Find_Global_Def_offs (int offs);
def_t *Find_Field_Def_name (const char *name);
def_t *Find_Field_Def_offs (int offs);

#endif//__def_h
