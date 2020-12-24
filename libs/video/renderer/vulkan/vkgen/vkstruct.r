#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>
#include <PropertyList.h>

#include "vkfielddef.h"
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

-(void) writeTable
{
	PLItem     *field_dict = [parse getObjectForKey:[self name]];
	PLItem     *new_name = [field_dict getObjectForKey:".name"];
	Array      *field_defs = [Array array];

	if ([parse string] == "skip") {
		return;
	}
	if (new_name) {
		outname = str_hold ([new_name string]);
	}

	if (field_dict) {
		PLItem     *field_keys = [field_dict allKeys];

		for (int i = [field_keys count]; i-- > 0; ) {
			string      field_name = [[field_keys getObjectAtIndex:i] string];

			if (str_mid(field_name, 0, 1) == ".") {
				continue;
			}
			PLItem     *field_item = [field_dict getObjectForKey:field_name];
			FieldDef   *field_def = [FieldDef fielddef:field_item
												struct:self
												 field:field_name];
			[field_defs addObject: field_def];
		}
	} else {
		for (int i = 0; i < type.strct.num_fields; i++) {
			qfot_var_t *field = &type.strct.fields[i];
			if (field.name == "sType" || field.name == "pNext") {
				continue;
			}
			FieldDef   *field_def = [FieldDef fielddef:nil
												struct:self
												 field:field.name];
			[field_defs addObject: field_def];
		}
	}
	for (int i = [field_defs count]; i-- > 0; ) {
		FieldDef   *field_def = [field_defs objectAtIndex:i];
		[field_def writeParseData];
	}
	fprintf (output_file, "static plfield_t %s_fields[] = {\n", [self outname]);
	for (int i = [field_defs count]; i-- > 0; ) {
		FieldDef   *field_def = [field_defs objectAtIndex:i];
		[field_def writeField];
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");

	fprintf (header_file, "int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context);\n",
			 [self outname]);
	fprintf (output_file, "int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context)\n",
			 [self outname]);
	fprintf (output_file, "{\n");
	fprintf (output_file,
			 "\treturn PL_ParseStruct (%s_fields, item, data, messages,"
			 " context);\n",
			 [self outname]);
	fprintf (output_file, "}\n");

	fprintf (output_file, "static exprsym_t %s_symbols[] = {\n", [self outname]);
	if (field_defs) {
		PLItem     *field_def;
		qfot_var_t *field;

		for (int i = [field_defs count]; i-- > 0; ) {
			FieldDef   *field_def = [field_defs objectAtIndex:i];
			[field_def writeSymbol];
		}
	} else {
		for (int i = 0; i < type.strct.num_fields; i++) {
			qfot_var_t *field = &type.strct.fields[i];
			if (field.name == "sType" || field.name == "pNext") {
				continue;
			}
			Type       *field_type = [Type findType: field.type];
			fprintf (output_file,
					 "\t{\"%s\", &%s, (void *) field_offset (%s, %s)},\n",
					 field.name, [field_type cexprType], [self outname], field.name);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");

	fprintf (output_file, "static exprtab_t %s_symtab = {\n", [self outname]);
	fprintf (output_file, "\t%s_symbols,\n", [self outname]);
	fprintf (output_file, "};\n");

	fprintf (output_file, "exprtype_t %s_type = {\n", [self outname]);
	fprintf (output_file, "\t\"%s\",\n", [self outname]);
	fprintf (output_file, "\tsizeof (%s),\n", [self outname]);
	fprintf (output_file, "\tcexpr_struct_binops,\n");
	fprintf (output_file, "\t0,\n");
	fprintf (output_file, "\t&%s_symtab,\n", [self outname]);
	fprintf (output_file, "};\n");
	fprintf (output_file, "\n");
	fprintf (header_file, "extern exprtype_t %s_type;\n", [self outname]);
}

-(void) writeSymtabInit
{
	PLItem     *field_dict = [parse getObjectForKey:[self outname]];

	if ([parse string] == "skip") {
		return;
	}

	fprintf (output_file, "\tcexpr_init_symtab (&%s_symtab, context);\n",
			 [self outname]);
}

-(string) outname
{
	if (outname) {
		return outname;
	}
	return [self name];
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
