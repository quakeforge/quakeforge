typedef enum {
	ex_statement,
	ex_expr,
	ex_def,
	ex_int,
	ex_float,
	ex_string,
	ex_vector,
	ex_quaternion,
} expr_type;

typedef struct estatement_s {
} estatement_t;

typedef struct expr_s {
	expr_type	type;
	union {
		struct {
			int		op;
			type_t	*type;
			struct expr_s *e1;
			struct expr_s *e2;
		}		expr;
		def_t	*def;
		int		int_val;
		float	float_val;
		string_t string_val;
		float	vector_val[3];
		float	quaternion_val[4];
		estatement_t *statement;
	} e;
} expr_t;

expr_t *new_expr (void);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
