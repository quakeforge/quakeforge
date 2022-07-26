/*
	class.h

	QC class support code

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/08

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __class_h
#define __class_h

typedef enum {
	ct_class,
	ct_category,
	ct_protocol,
} ct_type_t;

typedef struct class_type_s {
	ct_type_t   type;
	union {
		struct category_s *category;
		struct class_s    *class;
		struct protocol_s *protocol;
	} c;
} class_type_t;

typedef struct class_s {
	int         defined;
	int         interface_declared;
	const char *name;
	struct class_s *super_class;
	struct category_s *categories;
	struct symtab_s *ivars;
	struct methodlist_s *methods;
	struct protocollist_s *protocols;
	struct def_s *def;
	struct type_s *type;
	class_type_t class_type;
} class_t;

typedef struct category_s {
	struct category_s *next;
	const char *name;
	class_t    *class;
	int         defined;
	struct methodlist_s *methods;
	struct protocollist_s *protocols;
	struct def_s *def;
	class_type_t class_type;
} category_t;

typedef struct protocol_s {
	const char *name;
	struct methodlist_s *methods;
	struct protocollist_s *protocols;
	struct def_s *def;
	class_type_t class_type;
} protocol_t;

typedef struct protocollist_s {
	int         count;
	protocol_t **list;
} protocollist_t;

typedef struct static_instance_s {
	const char *class;
	struct def_s *instance;
} static_instance_t;

extern struct type_s type_id;
extern struct type_s type_object;
extern struct type_s type_class;
extern struct type_s type_Class;
extern struct type_s type_protocol;
extern struct type_s type_selector;
extern struct type_s type_SEL;
extern struct type_s type_IMP;
extern struct type_s type_supermsg;
extern struct type_s type_exec_class;
extern struct type_s type_method;
extern struct type_s type_super;
extern struct type_s type_method_description;
extern struct type_s type_category;
extern struct type_s type_ivar;
extern struct type_s type_module;

extern class_type_t *current_class;

extern int obj_initialized;

struct dstring_s;
struct expr_s;
struct method_s;
struct symbol_s;
struct selector_s;

int is_id (const struct type_s *type) __attribute__((pure));
int is_class (const struct type_s *type) __attribute__((pure));
int is_Class (const struct type_s *type) __attribute__((pure));
int is_classptr (const struct type_s *type) __attribute__((pure));
int is_SEL (const struct type_s *type) __attribute__((pure));
int is_object (const struct type_s *type) __attribute__((const));
int is_method (const struct type_s *type) __attribute__((const));
int is_method_description (const struct type_s *type) __attribute__((const));
int obj_types_assignable (const struct type_s *dst, const struct type_s *src);

class_t *extract_class (class_type_t *class_type) __attribute__((pure));
const char *get_class_name (class_type_t *class_type, int pretty);
struct symbol_s *class_symbol (class_type_t *class_type, int external);
void class_init (void);
class_t *get_class (struct symbol_s *sym, int create);
void class_add_methods (class_t *class, struct methodlist_s *methods);
void class_add_protocols (class_t *class, protocollist_t *protocols);
struct symtab_s *class_new_ivars (class_t *class);
void class_add_ivars (class_t *class, struct symtab_s *ivars);
void class_check_ivars (class_t *class, struct symtab_s *ivars);
void class_begin (class_type_t *class_type);
void class_finish (class_type_t *class_type);
int class_access (class_type_t *current_class, class_t *class) __attribute__((pure));
struct symbol_s *class_find_ivar (class_t *class, int vis, const char *name);
struct symtab_s *class_ivar_scope (class_type_t *class_type,
								   struct symtab_s *parent);
void class_finish_ivar_scope (class_type_t *class_type,
							  struct symtab_s *ivar_scope,
							  struct symtab_s *param_scope);
struct method_s *class_find_method (class_type_t *class_type,
									struct method_s *method);
struct method_s *class_message_response (struct type_s *clstype, int class_msg,
										 struct expr_s *sel);
struct symbol_s *class_pointer_symbol (class_t *class_type);
category_t *get_category (struct symbol_s *class_name,
						  const char *category_name, int create);
void category_add_methods (category_t *category, struct methodlist_s *methods);
void category_add_protocols (category_t *category, protocollist_t *protocols);
void class_finish_module (void);

struct symtab_s *class_to_struct (class_t *class, struct symtab_s *symtab);
void emit_class_ref (const char *class_name);
void emit_category_ref (const char *class_name, const char *category_name);

protocol_t *get_protocol (const char *name, int create);
void protocol_add_methods (protocol_t *protocol, struct methodlist_s *methods);
void protocol_add_protocols (protocol_t *protocol, protocollist_t *protocols);
struct def_s *protocol_def (protocol_t *protocol);
protocollist_t *new_protocol_list (void);
protocollist_t *add_protocol (protocollist_t *protocollist, const char *name);
int procollist_find_protocol (protocollist_t *protocollist, protocol_t *proto) __attribute__((pure));
struct method_s *protocollist_find_method (protocollist_t *protocollist,
										   struct selector_s *selector,
										   int nstance)
											__attribute__((pure));

int compare_protocols (protocollist_t *protos1, protocollist_t *protos2) __attribute__((pure));
void print_protocollist (struct dstring_s *dstr, protocollist_t *protocollist);
struct def_s *emit_protocol (protocol_t *protocol);
struct def_s *emit_protocol_list (protocollist_t *protocols, const char *name);

void clear_classes (void);

#endif//__class_h
