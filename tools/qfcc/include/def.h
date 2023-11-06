/*
	def.h

	def management and symbol tables

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/04

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

#ifndef __def_h
#define __def_h

#include "QF/progs/pr_comp.h"
#include "QF/progs/pr_debug.h"

#include "rua-lang.h"
#include "specifier.h"

/** \defgroup qfcc_def Def handling
	\ingroup qfcc
*/
///@{

struct symbol_s;
struct symtab_s;
struct expr_s;

/** Represent a memory location that holds a QuakeC/Ruamoko object.

	The object represented by the def may be of any type (either internally
	defined by qfcc (float, int, vector, etc) or user defined (structs, arrays
	etc)).

	Unless the def is external (ie, declared to be in another compilation
	unit), defs are always attached to a defspace. Storage for the def's
	object is allocated from that space.
*/
typedef struct def_s {
	struct def_s *next;			///< general purpose linking
	struct def_s *temp_next;	///< linked list of "free" temp defs

	struct type_s *type;		///< QC type of this def
	const char *name;			///< the def's name
	struct defspace_s *space;	///< defspace to which this def belongs
	int	        offset;			///< address of this def in its defspace
	int         reg;			///< base register index to access def

	/** \name Def aliasing.
		Aliasing a def provides a different view of the def providing access
		via a different type, or access to members of structs, unions and
		arrays.

		Alias defs are very simple: they store only the type and relative
		offset off the def they alias. All other information is held in the
		def they alias, including relocation records. However, they do keep
		track of the source file and line that first created the alias.

		The relations between a def and any of its aliases are maintained by
		a linked list headed by def_t::alias_defs and connected by
		def_t::next. def_t::alias is used to find the main def via one if its
		aliases. The order of the aliases in the list is arbitrary: it is the
		reverse of the order in which they were created.

		\dotfile vector-alias.dot "Accessing a vector via its components."
	*/
	//@{
	struct def_s   *alias_defs;	///< defs that alias this def
	struct def_s   *alias;		///< real def which this def aliases
	//@}
	struct reloc_s *relocs;		///< for relocations
	const struct expr_s *initializer;///< initialer expression
	struct daglabel_s *daglabel;///< daglabel for this def
	struct flowvar_s *flowvar;	///< flowvar for this def

	unsigned    offset_reloc:1;	///< use *_def_ofs relocs
	unsigned    initialized:1;	///< the def has been initialized
	unsigned    constant:1;		///< stores constant value
	unsigned    global:1;		///< globally declared def
	unsigned    external:1;		///< externally declared def
	unsigned    local:1;		///< function local def
	unsigned    param:1;		///< function param def
	unsigned    argument:1;		///< function argument def
	unsigned    system:1;		///< system def
	unsigned    nosave:1;		///< don't set DEF_SAVEGLOBAL

	rua_loc_t   loc;			///< declaring/defining source location

	int         qfo_def;		///< index to def in qfo defs

	void       *return_addr;	///< who allocated this
	void       *free_addr;		///< who freed this
} def_t;

/** Create a new def.

	Defs may be unnamed.

	Untyped defs (used by type encodings) never allocate space for the def.

	If \a storage is not sc_extern and \a type is not null, space of the
	size specified by \a type will be allocated to the def from the defspace
	specified by \a space.

	\param name		The name for the def. May be null for unnamed defs.
	\param type		The type for the def. May be null for untyped defs.
	\param space    The defspace to which the def belongs. Must not be null
					for if \a storage is not sc_extern.
	\param storage	The storage class for the def.
	\return			The new def.
*/
def_t *new_def (const char *name, struct type_s *type,
				struct defspace_s *space, storage_class_t storage);

/** Create a def that aliases another def.

	Aliasing a def to the same type is useless, but not checked. Aliasing a
	def to a type larger than the def's type will generate an internal error.

	If the offset is negative, or the offset plus the size of the aliasing type
	is greater than the size of the def's type, then an internal error will
	be generated.

	The alias def keeps track of its base def and is attached to the base def
	so any aliases of that def can be found.

	For any combination of type and offset, there will be only one alias def
	created.

	\param def		The def to be aliased.
	\param type		The type of the alias.
	\param offset	Offset of the alias relative to the def.
	\return			The def aliasing \a def.

	\todo Make aliasing to the same type a no-op?
*/
def_t *alias_def (def_t *def, struct type_s *type, int offset);
//FIXME this probably shouldn't exist (it's for swizzles, so doing proper
//multi-width swizzles will remove the need for it)
def_t *cover_alias_def (def_t *def, struct type_s *type, int offset);

/** Free a def.

	If the def has space allocated to it, the space will be freed.

	\param def		The def to be freed.
*/
void free_def (def_t *def);

/** \name Temporary defs

	Temporary defs are used for recycling memory for tempary variables. They
	always have names in the form of <code>.tmpN</code> where N is a
	non-negative integer. The type of the temporary def can change throughout
	its life, but the size will never change.

	Temporary defs are bound to the current function (::current_func must
	be valid). They are always allocated from the funciont's local defspace.
*/
///@{
/** Get a temporary def.

	If the current function has a free temp def of the same size as \a size,
	then that def will be recycled, otherwise a new temp will be created. When
	a new temp is created, its name is set to <code>.tmpN</code> where \c N
	comes from function_t::temp_num, which is then incremented.

	\note ::current_func must be valid.

	\param type		The type of the temporary variable.
	\return			The def for the temparary variable.

	\bug size of type must be 1 to 4.
	\todo support arbitrary sizes
*/
def_t *temp_def (struct type_s *type);

/** Free a tempary def so it may be recycled.

	The temp is put into a list of free temp defs maintained by ::current_func.

	\note ::current_func must be valid.

	\param temp		The temp def to be recycled.
*/
void free_temp_def (def_t *temp);
///@}

/** Initialize a vm def from a qfcc def.

	\param def		The qfcc def to copy.
	\param ddef		The vm def to be initialized.
	\param aux		Copy the field object's type rather than the def's.

	\bug The def is not verified to be a field def when \a aux is true.
*/
void def_to_ddef (def_t *def, ddef_t *ddef, int aux);

void init_vector_components (struct symbol_s *vector_sym, int is_field,
							 struct symtab_s *symtab);

/** Initialize a def referenced by the given symbol.

	The symbol is checked for redefinition. A symbol is considered to be
	redefined if the previous definition is in the same symbol table and
	of a different type or already initialized.

	If \a type is null, then the def will be given the default type (as
	specified by ::type_default).

	If an initializer expressions is given (\a init is not null), then it
	must be a constant expression (eg, 2 + 3) for non-local defs. Aggregate
	types (structs, arrays (unions?)) use block expressions where each
	expression in the block initializes one element of the aggregate. Nested
	aggregates simply use nested block expressions. As for simple types, each
	expression in a block expression must also be constant for non-local defs.

	For \a space and \a storage, see new_def().

	\param sym		The symbol for which to create and initialize a def.
	\param init		If not null, the expressions to use to initialize the def.
	\param space	The space from which to allocate space for the def.
	\param storage	The storage class of the def.
	\param symtab   The symbol table into which the def will be placed.
*/
void initialize_def (struct symbol_s *sym,
					 const struct expr_s *init, struct defspace_s *space,
					 storage_class_t storage, struct symtab_s *symtab);

/** Convenience function for obtaining a def's actual offset.

	Takes care of an alias def's relative offset.

	\param def		The def of which to obtain the offset. May be an alias def.
	\return			The actual offset of the def in the def's defspace.
*/
int def_offset (def_t *def) __attribute__((pure));

/** Convenience function for obtaining a def's size.

	Whether the def is an alias or not is irrelevant.

	\param def		The def of which to obtain the size.
	\return			The size of the def.
*/
int def_size (def_t *def) __attribute__((pure));

/** Visit all defs that alias the given def, including itself.

	First, the given def is visited, and then every candidate def connected
	to the given def via the aliasing links will be visited. Candidate defs
	are those that overlap the given def when \a overlap is true, otherwise
	every connected def is a candidate. \a overlap has no meaning if the
	given def is not an alias def as all alias defs connected to the given
	def are guaranteed to overlap with the given def. Any def will be visited
	at most once.

	The \a visit function may return non-zero to terminate the loop early.
	Any data needed by \a visit should be made available via \a data, which
	will be passed on to \a visit via the second parameter. The first
	parameter of \a visit is the def currently being visted.

	This function is useful also for defs that are not alias defs and do not
	have any aliases: \a visit will be called for the def and then the
	function will return.

	\param def		The def representing the alias cluster to visit.
	\param overlap  If non-zero, then only defs that overlap \a def will
					be visited. If 2, then the given def must fully overlap
					the visited def.
	\param visit	The function to call when visiting a def. The first
					parameter is the def being visited, and the second
					parameter is \a data passed on. If non-zero is returned,
					the pass through the alias cluster will terminate.
	\param data		Pointer to the data needed by \a visit.
	\return			The value returned by \a visit returned non-zero,
					otherwise 0.
*/
int def_visit_all (def_t *def, int overlap,
				   int (*visit) (def_t *, void *), void *data);

int def_visit_overlaps (def_t *def, int offset, int size, int overlap,
						def_t *skip,
						int (*visit) (def_t *, void *), void *data);

///@}

#endif//__def_h
