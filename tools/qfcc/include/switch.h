typedef struct switch_block_s {
	expr_t				*test;
	struct hashtab_s	*labels;
} switch_block_t;

typedef struct case_label_s {
	struct expr_s		*label;
	struct expr_s		*value;
} case_label_t;

struct expr_s *case_label_expr (switch_block_t *switch_block,
								struct expr_s *value);
switch_block_t *new_switch_block (void);
struct expr_s *switch_expr (switch_block_t *switch_block,
							struct expr_s *break_label,
							struct expr_s *statements);
