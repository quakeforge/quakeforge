#ifndef __qfprogs_h
#define __qfprogs_h

struct progs_s;
struct qfo_s;

extern int sorted;
extern int verbosity;

extern const char *reloc_names[];

void disassemble_progs (struct progs_s *pr);

void dump_globals (struct progs_s *pr);
void dump_fields (struct progs_s *pr);
void dump_functions (struct progs_s *pr);

void dump_lines (struct progs_s *pr);

void dump_modules (struct progs_s *pr);

struct dfunction_s *func_find (int st_num);

void dump_strings (struct progs_s *pr);

void qfo_globals (struct qfo_s *qfo);
void qfo_relocs (struct qfo_s *qfo);

#endif//__qfprogs_h
