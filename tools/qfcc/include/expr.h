typedef enum {
	ex_label,
	ex_block,
	ex_expr,	// binary expression
	ex_uexpr,	// unary expression
	ex_def,
	ex_temp,	// temporary variable
	ex_nil,		// umm, nil, null. nuff said

	ex_string,
	ex_float,
	ex_vector,
	ex_entity,
	ex_field,
	ex_func,
	ex_pointer,
	ex_quaternion,
	ex_integer,
} expr_type;

typedef struct {
	statref_t	*refs;
	dstatement_t *statement;
	char *name;
} elabel_t;

typedef struct {
	struct expr_s *head;
	struct expr_s **tail;
	struct expr_s *result;
	int			is_call;
} block_t;

typedef struct {
	struct expr_s *expr;
	def_t		*def;
	type_t		*type;
	int			users;
} temp_t;

typedef struct expr_s {
	struct expr_s *next;
	expr_type	type;
	int			line;
	string_t	file;
	int			paren;
	union {
		elabel_t label;
		block_t block;
		struct {
			int		op;
			type_t	*type;
			struct expr_s *e1;
			struct expr_s *e2;
		}		expr;
		def_t	*def;
		temp_t	temp;

		char	*string_val;
		float	float_val;
		float	vector_val[3];
		int		entity_val;
		int		field_val;
		int		func_val;
		int		pointer_val;
		float	quaternion_val[4];
		int		integer_val;
	} e;
} expr_t;

extern etype_t qc_types[];
extern struct type_s *types[];
extern expr_type expr_types[];

type_t *get_type (expr_t *e);
etype_t extract_type (expr_t *e);

expr_t *new_expr (void);
expr_t *new_label_expr (void);
expr_t *new_block_expr (void);
expr_t *new_binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *new_unary_expr (int op, expr_t *e1);
expr_t *new_temp_def_expr (type_t *type);

void inc_users (expr_t *e);

expr_t *append_expr (expr_t *block, expr_t *e);

void print_expr (expr_t *e);

void convert_int (expr_t *e);

expr_t *test_expr (expr_t *e, int test);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *asx_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
expr_t *function_expr (expr_t *e1, expr_t *e2);
expr_t *return_expr (function_t *f, expr_t *e);
expr_t *conditional_expr (expr_t *cond, expr_t *e1, expr_t *e2);
expr_t *incop_expr (int op, expr_t *e, int postop);

def_t *emit_statement (int line, opcode_t *op, def_t *var_a, def_t *var_b, def_t *var_c);
void emit_expr (expr_t *e);

expr_t *error (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));
void warning (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));

const char *get_op_string (int op);

extern int lineno_base;
