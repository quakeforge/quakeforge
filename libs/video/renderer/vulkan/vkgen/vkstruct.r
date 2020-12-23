#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>
#include <PropertyList.h>

#include "vkgen.h"
#include "vkstruct.h"

@implementation Struct

-(string) name
{
	return str_mid(type.strct.tag, 4);
}

-(void) addToQueue
{
	string name = [self name];
	if (!Hash_Find (processed_types, name)) {
		printf ("    +%s\n", name);
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

-(void) forEachFieldCall: (varfunc) func
{
	qfot_struct_t *strct =&type.strct;

	for (int i = 0; i < strct.num_fields; i++) {
		func (&strct.fields[i]);
	}
}

-(qfot_var_t *)findField:(string) fieldName
{
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (var.name == fieldName) {
			return var;
		}
	}
	return nil;
}

-(void) writeTable: (PLItem *) parse
{
	string      name = [self name];
	PLItem     *field_dict = [parse getObjectForKey:name];
	PLItem     *field_defs = [field_dict allKeys];
	Type       *field_type;
	PLItem     *new_name = [field_dict getObjectForKey:".name"];

	if (new_name) {
		name = [new_name string];
	}

	if (field_defs) {
		PLItem     *field_def;
		qfot_var_t *field;

		for (int i = [field_defs count]; i-- > 0; ) {
			string field_name = [[field_defs  getObjectAtIndex:i] string];
			field_def = [field_dict getObjectForKey:field_name];
			PLItem     *type_desc = [field_def getObjectForKey:"type"];
			string      type_record;
			string      type_type;
			string      size_field = nil;
			string      value_field = nil;

			if (!type_desc || str_mid(field_name, 0, 1) == ".") {
				continue;
			}
			type_record = [[type_desc getObjectAtIndex:0] string];
			type_type = [[type_desc getObjectAtIndex:1] string];

			field_type = [[Type lookup: type_type] dereference];
			fprintf (output_file, "static parse_%s_t parse_%s_%s_data = {\n",
					 type_record, name, field_name);
			if (type_record == "single") {
				fprintf (output_file, "\t%s,\n", [field_type parseType]);
				fprintf (output_file, "\tsizeof (%s),\n", type_type);
				fprintf (output_file, "\tparse_%s,\n", type_type);
				value_field = [[field_def getObjectForKey:"value"] string];
				fprintf (output_file, "\tfield_offset (%s, %s),\n",
						 name, value_field);
			} else if (type_record == "array") {
				fprintf (output_file, "\t%s,\n", [field_type parseType]);
				fprintf (output_file, "\tsizeof (%s),\n", type_type);
				fprintf (output_file, "\tparse_%s,\n", type_type);
				value_field = [[field_def getObjectForKey:"values"] string];
				size_field = [[field_def getObjectForKey:"size"] string];
				fprintf (output_file, "\tfield_offset (%s, %s),\n",
						 name, value_field);
				if (size_field) {
					fprintf (output_file, "\tfield_offset (%s, %s),\n",
							 name, size_field);
				} else {
					fprintf (output_file, "\t-1,\n");
				}
			} else if (type_record == "data") {
				value_field = [[field_def getObjectForKey:"data"] string];
				size_field = [[field_def getObjectForKey:"size"] string];
				fprintf (output_file, "\tfield_offset (%s, %s),\n",
						 name, value_field);
				if (size_field) {
					fprintf (output_file, "\tfield_offset (%s, %s),\n",
							 name, size_field);
				} else {
					fprintf (output_file, "\t-1,\n");
				}
			} else {
				fprintf (output_file, "\tno type,\n");
			}
			fprintf (output_file, "};\n");
		}
	}
	fprintf (output_file, "static plfield_t %s_fields[] = {\n", name);
	if (field_defs) {
		PLItem     *field_def;
		qfot_var_t *field;

		for (int i = [field_defs count]; i-- > 0; ) {
			string field_name = [[field_defs  getObjectAtIndex:i] string];
			if (str_mid(field_name, 0, 1) == ".") {
				continue;
			}
			field_def = [field_dict getObjectForKey:field_name];
			if ([field_def string] == "auto") {
				field = [self findField:field_name];
				if (!field) {
					continue;
				}
				field_type = [Type findType: field.type];
				fprintf (output_file,
						 "\t{\"%s\", field_offset (%s, %s), %s, %s, %s},\n",
						 field_name, name, field_name,
						 [field_type parseType], [field_type parseFunc],
						 [field_type parseData]);
			} else {
				PLItem     *type_desc = [field_def getObjectForKey:"type"];
				string      type_record;
				string      type_type;
				string      parseType = "no type";

				type_record = [[type_desc getObjectAtIndex:0] string];
				type_type = [[type_desc getObjectAtIndex:1] string];

				field_type = [[Type lookup: type_type] dereference];
				if (type_record == "single") {
					parseType = [field_type parseType];
				} else if (type_record == "array") {
					parseType = "QFArray";
				} else if (type_record == "data") {
					parseType = "QFBinary";
				}
				fprintf (output_file,
						 "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
						 field_name, parseType, type_record,
						 name, field_name);
			}
		}
	} else {
		for (int i = 0; i < type.strct.num_fields; i++) {
			qfot_var_t *field = &type.strct.fields[i];
			if (field.name == "sType" || field.name == "pNext") {
				continue;
			}
			field_type = [Type findType: field.type];
			fprintf (output_file,
					 "\t{\"%s\", field_offset (%s, %s), %s, %s, %s},\n",
					 field.name, name, field.name,
					 [field_type parseType], [field_type parseFunc],
					 [field_type parseData]);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");

	fprintf (header_file, "int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context);\n",
			 name);
	fprintf (output_file, "int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context)\n",
			 name);
	fprintf (output_file, "{\n");
	fprintf (output_file,
			 "\treturn PL_ParseDictionary (%s_fields, item, data, messages,"
			 " context);\n",
			 name);
	fprintf (output_file, "}\n");

	fprintf (output_file, "static exprsym_t %s_symbols[] = {\n", name);
	if (field_defs) {
		PLItem     *field_def;
		qfot_var_t *field;

		for (int i = [field_defs count]; i-- > 0; ) {
			string field_name = [[field_defs  getObjectAtIndex:i] string];
			field_def = [field_dict getObjectForKey:field_name];
			PLItem     *type_desc = [field_def getObjectForKey:"type"];
			string      type_record;
			string      type_type;
			string      size_field = nil;
			string      value_field = nil;

			if (str_mid(field_name, 0, 1) == ".") {
				continue;
			}
			field_def = [field_dict getObjectForKey:field_name];
			if ([field_def string] == "auto") {
				field = [self findField:field_name];
				if (!field) {
					continue;
				}
				field_type = [Type findType: field.type];
				fprintf (output_file,
						 "\t{\"%s\", &%s, (void *) field_offset (%s, %s)},\n",
						 field_name, [field_type cexprType], name, field_name);
			} else {
				type_record = [[type_desc getObjectAtIndex:0] string];
				if (type_record == "single") {
					value_field = [[field_def getObjectForKey:"value"] string];
				} else if (type_record == "array") {
					value_field = [[field_def getObjectForKey:"values"] string];
				} else if (type_record == "data") {
					value_field = [[field_def getObjectForKey:"data"] string];
				}
				if (!value_field) {
					value_field = field_name;
				}
				fprintf (output_file,
						 "\t{\"%s\", 0/*FIXME*/,"
						 " (void *) field_offset (%s, %s)},\n",
						 field_name, name, value_field);
			}
		}
	} else {
		for (int i = 0; i < type.strct.num_fields; i++) {
			qfot_var_t *field = &type.strct.fields[i];
			if (field.name == "sType" || field.name == "pNext") {
				continue;
			}
			field_type = [Type findType: field.type];
			fprintf (output_file,
					 "\t{\"%s\", &%s, (void *) field_offset (%s, %s)},\n",
					 field.name, [field_type cexprType], name, field.name);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");

	fprintf (output_file, "static exprtab_t %s_symtab = {\n", name);
	fprintf (output_file, "\t%s_symbols,\n", name);
	fprintf (output_file, "};\n");

	fprintf (output_file, "exprtype_t %s_type = {\n", name);
	fprintf (output_file, "\t\"%s\",\n", name);
	fprintf (output_file, "\tsizeof (%s),\n", name);
	fprintf (output_file, "\tcexpr_struct_binops,\n");
	fprintf (output_file, "\t0,\n");
	fprintf (output_file, "\t&%s_symtab,\n", name);
	fprintf (output_file, "};\n");
	fprintf (header_file, "extern exprtype_t %s_type;\n", name);
}

-(void) writeSymtabInit:(PLItem *) parse
{
	string      name = [self name];
	PLItem     *field_dict = [parse getObjectForKey:name];
	PLItem     *new_name = [field_dict getObjectForKey:".name"];

	if (new_name) {
		name = [new_name string];
	}

	fprintf (output_file, "\tcexpr_init_symtab (&%s_symtab, context);\n", name);
}

-(string) cexprType
{
	return [self name] + "_type";
}

-(string) parseType
{
	return "QFDictionary";
}

-(string) parseFunc
{
	return "fix me";
}

-(string) parseData
{
	return "0";
}
@end
