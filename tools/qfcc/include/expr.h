enum expr_type {
	ex_expr,
	ex_def,
	ex_int,
	ex_float,
	ex_string,
	ex_vector,
	ex_quaternion,
};

typedef struct expr_s {
	int		type;
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
	} e;
} expr_t;

expr_t *new_expr (void);
