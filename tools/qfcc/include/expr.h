typedef enum {
	ex_label,
	ex_block,
	ex_expr,	// binary expression
	ex_uexpr,	// unary expression
	ex_def,
	ex_int,
	ex_float,
	ex_string,
	ex_vector,
	ex_quaternion,
} expr_type;

typedef struct {
	statref_t	*refs;
	dstatement_t *statement;
	char *name;
} label_t;

typedef struct {
	struct expr_s *head;
	struct expr_s **tail;
} block_t;

typedef struct expr_s {
	struct expr_s *next;
	expr_type	type;
	union {
		label_t label;
		block_t block;
		struct {
			int		op;
			type_t	*type;
			struct expr_s *e1;
			struct expr_s *e2;
		}		expr;
		def_t	*def;
		int		int_val;
		float	float_val;
		char	*string_val;
		float	vector_val[3];
		float	quaternion_val[4];
	} e;
} expr_t;

expr_t *new_expr (void);
expr_t *new_label_expr (void);
expr_t *new_block_expr (void);
expr_t *new_binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *new_unary_expr (int op, expr_t *e1);

expr_t *append_expr (expr_t *block, expr_t *e);

void print_expr (expr_t *e);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
expr_t *function_expr (expr_t *e1, expr_t *e2);

void emit_expr (expr_t *e);
