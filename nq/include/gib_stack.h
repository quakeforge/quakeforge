#define GIB_LOCALS gib_substack[gib_subsp - 1].local
#define GIB_CURRENTMOD gib_substack[gib_subsp - 1].mod
#define GIB_CURRENTSUB gib_substack[gib_subsp - 1].sub

typedef struct gib_instack_s
{
	gib_inst_t *instruction;
	char **argv;
	int argc;
} gib_instack_t;

typedef struct gib_substack_s
{
	gib_module_t *mod;
	gib_sub_t *sub;
	gib_var_t *local;
} gib_substack_t;

extern gib_instack_t *gib_instack;
extern gib_substack_t *gib_substack;

extern int gib_insp;
extern int gib_subsp;

void GIB_InStack_Push (gib_inst_t *instruction, int argc, char **argv);
void GIB_InStack_Pop ();
void GIB_SubStack_Push (gib_module_t *mod, gib_sub_t *sub, gib_var_t *local);
void GIB_SubStack_Pop ();

