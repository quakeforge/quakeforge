typedef int (*gib_func_t) (void);

typedef struct gib_var_s
{
        char *key;  
        char *value;
        struct gib_var_s *next;
} gib_var_t;


typedef struct gib_sub_s
{
	char *name;
	char *code;
	gib_var_t *vars;
	struct gib_sub_s *next;
} gib_sub_t;

typedef struct gib_module_s
{
	char *name;
	gib_sub_t *subs;
	gib_var_t *vars;
	struct gib_module_s *next;
} gib_module_t;

typedef struct gib_inst_s
{
	char *name;
	gib_func_t func;
	struct gib_inst_s *next;
} gib_inst_t;

void GIB_Init (void);
void GIB_Gib_f (void);
void GIB_Load_f (void);
void GIB_Stats_f (void);

