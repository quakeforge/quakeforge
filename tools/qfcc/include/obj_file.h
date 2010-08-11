/*
	obj_file.h

	object file support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/6/16

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

#ifndef __obj_file_h
#define __obj_file_h

/** \defgroup qfcc_qfo Object file functions
	\ingroup qfcc
*/
//@{

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"
#include "QF/quakeio.h"

/** Identifier string for qfo object files (includes terminating nul)

	\hideinitializer
*/
#define QFO			"QFO"
/** QFO object file format version (MMmmmRRR 0.001.005 (hex))

	\hideinitializer
*/
#define QFO_VERSION 0x00001005

/** Header block of QFO object files. The sections of the object file
	come immediately after the header, and are always in the order given by
	the struct.

	All indeces to records are 0-based from the beginning of the relevant
	section.
*/
typedef struct qfo_header_s {
	int8_t      qfo[4];			///< identifier string (includes nul) (#QFO)
	pr_int_t    version;		///< QFO format version (#QFO_VERSION)
	pr_int_t    code_size;		///< number of instructions in code section
	pr_int_t    data_size;		///< number of words in data section
	pr_int_t    far_data_size;	///< number of words in far data section
	pr_int_t    strings_size;	///< number of chars in strings section
	pr_int_t    num_relocs;		///< number of relocation records
	pr_int_t    num_defs;		///< number of def records
	pr_int_t    num_funcs;		///< number of function records
	pr_int_t    num_lines;		///< number of line records
	pr_int_t    types_size;		///< number of chars in type strings section
	/** Number of entity field words defined by the object file. There is no
		corresponding section in the object file.
	*/
	pr_int_t    entity_fields;
} qfo_header_t;

/** Representation of a def in the object file.
*/
typedef struct qfo_def_s {
	etype_t     basic_type;		///< type of def as seen by the VM
	string_t    full_type;		///< full type string, encoded by encode_type()
	string_t    name;			///< def name
	pr_int_t    ofs;			///< def offset (address)

	pr_int_t    relocs;			///< index of first reloc record
	pr_int_t    num_relocs;		///< number of reloc records

	pr_uint_t   flags;			///< \ref qfcc_qfo_QFOD "QFOD flags"

	string_t    file;			///< source file name
	pr_int_t    line;			///< source line number
} qfo_def_t;
///@}

/** \defgroup qfcc_qfo_QFOD QFOD flags
	\ingroup qfcc_qfo
*/
///@{

/** The def has been initialized.

	\hideinitializer
*/
#define QFOD_INITIALIZED	(1u<<0)

/** The def is a shared constant and must not be modified.

	\hideinitializer
*/
#define QFOD_CONSTANT		(1u<<1)

/** The def offset is an absolute address.

	\hideinitializer
*/
#define QFOD_ABSOLUTE		(1u<<2)

/** The def is visible to other object files.

	\hideinitializer
*/
#define QFOD_GLOBAL			(1u<<3)

/** The def is undefined and need to be supplied by another object file.

	\hideinitializer
*/
#define QFOD_EXTERNAL		(1u<<4)

/** The def is local to a function and is not visible to other functions or
	object files.

	\hideinitializer
*/
#define QFOD_LOCAL			(1u<<5)

/** The def is a system def and needs to be allocated before other defs

	\hideinitializer
*/
#define QFOD_SYSTEM			(1u<<6)

/** The def does not need to be saved when saving game state.

	\hideinitializer
*/
#define QFOD_NOSAVE			(1u<<7)
///@}

/** \ingroup qfcc_qfo
*/
///@{

/** Representation of a function in the object file.
*/
typedef struct qfo_func_s {
	string_t    name;			///< function name
	string_t    file;			///< source file name
	pr_int_t    line;			///< source line number

	/** \name Function code location.
		If #code is 0, or #builtin is non-zero, then the function is a VM
		builtin function.
		If both #code and #builtin are 0, then the function is a VM builtin
		function and the VM resolves the function number using the function
		name.
	*/
	///@{
	pr_int_t    builtin;		///< VM builtin function number
	pr_int_t    code;			///< Address in the code section of the first
								///< instruction of the function.
	///@}

	pr_int_t    def;			///< def that references this function. Index
								///< to ::qfo_def_t. The data word pointed to
								///< by the def stores the index of this
								///< function.

	/** \name Function local data.
	*/
	///@{
	pr_int_t    locals_size;	///< Number of words of local data reqired by
								///< the function.
	pr_int_t    local_defs;		///< Index to the first ::qfo_def_t def record
								///< representing the functions local
								///< variables.
	pr_int_t    num_local_defs;	///< Number of local def records.
	///@}

	pr_int_t    line_info;		///< Index to first ::pr_lineno_t line record.
								///< Zero if there are no records.

	/** \name Function parameters.
	*/
	///@{
	pr_int_t    num_parms;		///< Number of parameters this function
								///< accepts. Maximum number is defined by
								///< #MAX_PARMS. Negative numbers give the
								///< minumum number of parameters by
								///< \f$-num\_parms - 1\f$
	byte        parm_size[MAX_PARMS]; ///< Number of words used by each
								///< parameter.
	///@}

	/** \name Function relocation records.
		XXX not sure how these work
	*/
	///@{
	pr_int_t    relocs;			///< Index to first ::qfo_reloc_t reloc record.
	pr_int_t    num_relocs;		///< Number of reloc records.
	///@}
} qfo_func_t;

/** Evil source of many headaches. The whole reason I've started writing this
	documentation.
	relocs are always in the order:
		referenced relocs
		unreferenced relocs

	For \c ref_op_* relocation types, \c ofs is the code section address of the
	statement that needs to be adjusted.
	
	For \c rel_def_* relocation types,
	\c ofs refers to the data section address of the word that needs to be
	adjusted.

	For \c ref_*_def(_ofs) relocation types, \c def is the index of the
	referenced def.
	
	For \c ref_*_op relocation types, \c def is the address of
	the referenced statement.
	
	For \c ref_*_string relocation types, \c def is
	always 0.
	
	For \c ref_*_field(_ofs) relocation types, \c def is the index of
	the referenced field def.
*/
typedef struct qfo_reloc_s {
	pr_int_t    ofs;			///< offset of the relocation
	pr_int_t    type;			///< type of the relocation (::reloc_type)
	pr_int_t    def;			///< "def" this relocation is for
} qfo_reloc_t;

/** In-memory representation of a QFO object file.
*/
typedef struct qfo_s {
	dstatement_t *code;
	int         code_size;
	pr_type_t  *data;
	int         data_size;
	pr_type_t  *far_data;
	int         far_data_size;
	char       *strings;
	int         strings_size;
	qfo_reloc_t *relocs;
	int         num_relocs;
	qfo_def_t  *defs;
	int         num_defs;
	qfo_func_t *funcs;
	int         num_funcs;
	pr_lineno_t *lines;
	int         num_lines;
	char       *types;
	int         types_size;
	int         entity_fields;
} qfo_t;
///@}

/** \defgroup qfcc_qfo_data_access QFO Data Acess
	\ingroup qfcc_qfo
	Macros for accessing data in the QFO address space
*/
///@{

/** \internal
	\param q pointer to ::qfo_t struct
	\param t typename prefix (see pr_type_u)
	\param o offset into object file data space
	\return lvalue of the appropriate type

	\hideinitializer
*/
#define QFO_var(q, t, o)	((q)->data[o].t##_var)

/** Access a float variable in the object file. Can be assigned to.

	\par QC type:
		\c float
	\param q pointer to ::qfo_t struct
	\param o offset into object file data space
	\return float lvalue

	\hideinitializer
*/
#define	QFO_FLOAT(q, o)		QFO_var (q, float, o)

/** Access a integer variable in the object file. Can be assigned to.

	\par QC type:
		\c integer
	\param q pointer to ::qfo_t struct
	\param o offset into object file data space
	\return int lvalue

	\hideinitializer
*/
#define	QFO_INT(q, o)		QFO_var (q, integer, o)

/** Access a vector variable in the object file. Can be assigned to.

	\par QC type:
		\c vector
	\param q pointer to ::qfo_t struct
	\param o offset into object file data space
	\return vec3_t lvalue

	\hideinitializer
*/
#define	QFO_VECTOR(q, o)	QFO_var (q, vector, o)

/** Access a string index variable in the object file. Can be assigned to.

	\par QC type:
		\c string
	\param q pointer to ::qfo_t struct
	\param o offset into object file data space
	\return string_t lvalue

	\hideinitializer
*/
#define	QFO_STRING(q, o)	QFO_var (q, string, o)

/** Retrieve a string from the object file, converting it to a C string.

	\param q pointer to ::qfo_t struct
	\param s offset into object file string space
	\return (char *)

	\hideinitializer
*/
#define QFO_GETSTR(q, s)	((q)->strings + (s))

/** Retrieve a type string from the object file, converting it to a C string.

	\param q pointer to ::qfo_t struct
	\param s offset into object file type string space
	\return (char *)

	\hideinitializer
*/
#define QFO_TYPESTR(q, s)	((q)->types + (s))

/** Access a string global, converting it to a C string.

	\param q pointer to ::qfo_t struct
	\param o offset into object file data space
	\return (char *)

	\hideinitializer
*/
#define QFO_GSTRING(q, o)	(QFO_GETSTR (q, (QFO_STRING (q, o))))

/** Access a function variable in the object file. Can be assigned to.

	\par QC type:
		\c void ()
	\param q pointer to ::qfo_t struct
	\param o offset into object file data space
	\return func_t lvalue

	\hideinitializer
*/
#define	QFO_FUNCTION(q, o)	QFO_var (q, func, o)

/** Access a pointer variable in the object file. Can be assigned to.

	\par QC type:
		\c void []
	\param q pointer to ::qfo_t struct
	\param t C type of the structure
	\param o offset into object file data space
	\return pointer_t lvalue

	\hideinitializer
*/
#define QFO_POINTER(q, t,o)	((t *)((q)->data + o))

/** Access a structure variable in the object file. Can be assigned to.

	\par QC type:
		\c void [](?)
	\param q pointer to ::qfo_t struct
	\param t C type of the structure
	\param o offset into object file data space
	\return structure lvalue. use & to make a pointer of the appropriate type.

	\hideinitializer
*/
#define QFO_STRUCT(q, t,o)	(*QFO_POINTER (q, t, o))

///@}

/** \ingroup qfcc_qfo
*/
///@{

struct pr_info_s;

/** Convert ::pr_info_t structure to ::qfo_t.
	\param pr pointer to ::pr_info_t struct
	\return pointer to new ::qfo_t struct, or 0 on error.
*/
qfo_t *qfo_from_progs (struct pr_info_s *pr);

/** Write a ::qfo_t struct to the named file.
	\param qfo pointer to ::qfo_t struct to write
	\param filename name of the file to write
	\return 0 for success, -1 for error.
*/
int qfo_write (qfo_t *qfo, const char *filename);

/** Read a ::qfo_t strcut from a ::QFile stream.
	\param file ::QFile stream to read from
	\return pointer to new ::qfo_t struct, or 0 on error.
*/
qfo_t *qfo_read (QFile *file);

/** Wrapper around qfo_read() to allow reading from a named file.
	\param filename name of the file to read
	\return pointer to new ::qfo_t struct, or 0 on error.
*/
qfo_t *qfo_open (const char *filename);

/** Convert ::qfo_t struct to ::pr_info_t struct.
	\param qfo pointer to ::qfo_t struct
	\param pr pointer to ::pr_info_t struct
	\return 0 for success, -1 for error
*/
int qfo_to_progs (qfo_t *qfo, struct pr_info_s *pr);

/** Create a new ::qfo_t struct
	\return pointer to new ::qfo_t struct, or 0 on error.
*/
qfo_t *qfo_new (void);

/** Add a block of code to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param code pointer to beginning of code block
	\param code_size number of instructions in the code block
*/
void qfo_add_code (qfo_t *qfo, dstatement_t *code, int code_size);

/** Add a block of data to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param data pointer to beginning of data block
	\param data_size number of data words in the data block
*/
void qfo_add_data (qfo_t *qfo, pr_type_t *data, int data_size);

/** Add a block of far data to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param far_data pointer to beginning of far data block
	\param far_data_size number of data words in the far data block
*/
void qfo_add_far_data (qfo_t *qfo, pr_type_t *far_data, int far_data_size);

/** Add a block of string data to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param strings pointer to beginning of string data block
	\param strings_size number of characters in the string data block
*/
void qfo_add_strings (qfo_t *qfo, const char *strings, int strings_size);

/** Add a block of relocation records to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param relocs pointer to first relocation record
	\param num_relocs number of relocation records
*/
void qfo_add_relocs (qfo_t *qfo, qfo_reloc_t *relocs, int num_relocs);

/** Add a block of def records to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param defs pointer to first def record
	\param num_defs number of def records
*/
void qfo_add_defs (qfo_t *qfo, qfo_def_t *defs, int num_defs);

/** Add a block of function records to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param funcs pointer to first function record
	\param num_funcs number of function records
*/
void qfo_add_funcs (qfo_t *qfo, qfo_func_t *funcs, int num_funcs);

/** Add a block of line records to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param lines pointer to first line record
	\param num_lines number of line records
*/
void qfo_add_lines (qfo_t *qfo, pr_lineno_t *lines, int num_lines);

/** Add a block of type string data to a ::qfo_t struct.
	\param qfo ::qfo_t struct to add to
	\param types pointer to beginning of string data block
	\param types_size number of characters in the type string data block
*/
void qfo_add_types (qfo_t *qfo, const char *types, int types_size);

/** Delete a ::qfo_t struct, as well as any substructure data.
	\param qfo ::qfo_t struct to delete
*/
void qfo_delete (qfo_t *qfo);

//@}

#endif//__obj_file_h
