typedef struct struct_field_s {
	struct struct_field_s *next;
	const char		*name;
	struct type_s	*type;
	int				offset;
} struct_field_t;

struct_field_t *new_struct_field (struct type_s *strct, struct type_s *type,
								  const char *name);
struct_field_t *struct_find_field (struct type_s *strct, const char *name);
struct type_s *new_struct (const char *name);
struct type_s *find_struct (const char *name);
