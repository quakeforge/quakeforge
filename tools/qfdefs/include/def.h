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

#endif//__def_h
