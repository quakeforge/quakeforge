/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

	$Id$
*/

#ifndef __class_h
#define __class_h

typedef struct class_s {
	int         defined;
	const char *class_name;
	const char *category_name;
	struct class_s *super_class;
	struct type_s *ivars;
	struct methodlist_s *methods;
	struct protocollist_s *protocols;
	struct def_s *def;
	struct type_s *type;
} class_t;

extern class_t  class_id;
extern class_t  class_Class;
extern class_t  class_Protocol;

extern class_t *current_class;

struct expr_s;
struct method_s;
struct protocol_s;
struct type_s;

struct def_s *class_def (class_t *class, int external);
void class_init (void);
class_t *get_class (const char *name, int create);
void class_add_methods (class_t *class, struct methodlist_s *methods);
void class_add_protocol_methods (class_t *class, struct expr_s *protocols);
void class_add_protocol (class_t *class, struct protocol_s *protocol);
void class_add_ivars (class_t *class, struct type_s *ivars);
void class_check_ivars (class_t *class, struct type_s *ivars);
void class_begin (class_t *class);
void class_finish (class_t *class);
struct struct_field_s *class_find_ivar (class_t *class, int protected,
										const char *name);
struct expr_s *class_ivar_expr (class_t *class, const char *name);
struct method_s *class_find_method (class_t *class, struct method_s *method);
struct method_s *class_message_response (class_t *class, struct expr_s *sel);
struct def_s *class_pointer_def (class_t *class);
class_t *get_category (const char *class_name, const char *category_name,
					   int create);
void class_finish_module (void);

typedef struct protocol_s {
	const char *name;
	struct methodlist_s *methods;
	struct protocollist_s *protocols;
} protocol_t;

typedef struct protocollist_s {
	int         count;
	protocol_t **list;
} protocollist_t;

protocol_t *get_protocol (const char *name, int create);
void protocol_add_methods (protocol_t *protocol, struct methodlist_s *methods);
void protocol_add_protocol_methods (protocol_t *protocol,
									struct expr_s *protocols);
struct def_s *protocol_def (protocol_t *protocol);
protocollist_t *new_protocollist (void);
void add_protocol (protocollist_t *protocollist, protocol_t *protocol);

struct def_s *emit_protocol (protocol_t *protocol);
struct def_s *emit_protocol_list (protocollist_t *protocols, const char *name);

void clear_classes (void);

#endif//__class_h
