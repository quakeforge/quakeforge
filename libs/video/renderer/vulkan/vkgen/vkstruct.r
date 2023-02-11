#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>
#include <PropertyList.h>

#include "vkfielddef.h"
#include "vkfieldtype.h"
#include "vkgen.h"
#include "vkstruct.h"

@implementation Struct

-(void) dealloc
{
	str_free (outname);
	str_free (label_field);
}

-(string) name
{
	return str_mid(type.strct.tag, 4);
}

-(void) addToQueue
{
	string name = [self name];
	if (!Hash_Find (processed_types, name)) {
		//printf ("    +%s\n", name);
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

-(void) queueFieldTypes
{
	qfot_struct_t *strct =&type.strct;
	PLItem     *field_dict = [parse getObjectForKey:[self name]];
	int         readonly = [field_dict string] == "readonly";

	if (readonly) {
		return;
	}

	for (int i = 0; i < strct.num_fields; i++) {
		qfot_var_t *var = &strct.fields[i];
		if (field_dict) {
			PLItem     *item = [field_dict getObjectForKey:var.name];
			FieldDef   *def = [FieldDef fielddef:item
										  struct:self
										   field:var.name];
			if (![def searchType]) {
				continue;
			}
		}
		Type       *type = [Type findType:var.type];
		[type addToQueue];
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

-(string)sTypeName
{
	string s = "VK_STRUCTURE_TYPE";
	string      name = [self outname];
	int         length = strlen (name);
	int         start, end, c;
	for (start = 2; start < length; start = end) {
		for (end = start + 1; end < length; end++) {
			c = str_char (name, end);
			if (c >= 'A' && c <= 'Z') {
				break;
			}
		}
		s += "_" + str_mid (name, start, end);
	}
	str_free (name);
	return str_upper (s);
}

-(void) setLabelField:(string)label_field
{
	self.label_field = label_field;
}

-(void) writeForward
{
	PLItem     *field_dict = [parse getObjectForKey:[self name]];
	int         readonly = [field_dict string] == "readonly";
	if (!readonly) {
		fprintf (output_file, "static int %s (const plfield_t *field,"
				 " const plitem_t *item, void *data, plitem_t *messages,"
				 " void *context);\n",
				 [self parseFunc]);
	}
}

static void
write_function_head (Struct *self)
{
	fprintf (output_file, "static int %s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context)\n",
			 [self parseFunc]);
	fprintf (output_file, "{\n");
}

static void
write_function_tail (Struct *self)
{
	fprintf (output_file, "}\n");
}

static void
write_type (Struct *self, PLItem *field_dict, string type)
{
	string      key = nil;
	switch (type) {
		case "QFDictionary": key = ".dictionary"; break;
		case "QFArray":      key = ".array";      break;
		case "QFBinary":     key = ".binary";     break;
		case "QFString":     key = ".string";     break;
	}
	PLItem     *type_obj = [field_dict getObjectForKey:key];
	int         count = [type_obj numKeys];
	if (!count) {
		//FIXME errors
		return;
	}
	fprintf (output_file, "\tif (type == %s) {\n", type);
	for (int i = 0; i < count; i++) {
		string      field = [type_obj keyAtIndex:i];
		PLItem     *item = [type_obj getObjectForKey:field];
		string      str = [item string];

		switch (str) {
			case "$item.string":
				str = "vkstrdup (context, PL_String (item))";
				break;
			case "$item.line":
				str = "PL_Line (item)";
				break;
			case "$name":
				str = "vkstrdup (context, field->name)";
				break;
			case "$index":
				str = "field->offset";
				break;
		}
		fprintf (output_file, "\t\t((%s *) data)->%s = %s;\n", [self outname],
				 field, str);
	}
	fprintf (output_file, "\t\treturn 1;\n");
	fprintf (output_file, "\t}\n");
}

static void
write_cexpr (Struct *self, Array *field_defs)
{
	fprintf (output_file, "static exprsym_t %s_symbols[] = {\n",
			 [self outname]);
	if (field_defs) {
		PLItem     *field_def;
		qfot_var_t *field;

		for (int i = [field_defs count]; i-- > 0; ) {
			FieldDef   *field_def = [field_defs objectAtIndex:i];
			[field_def writeSymbol];
		}
	} else {
		for (int i = 0; i < self.type.strct.num_fields; i++) {
			qfot_var_t *field = &self.type.strct.fields[i];
			if (field.name == "sType" || field.name == "pNext") {
				continue;
			}
			Type       *field_type = [Type findType: field.type];
			fprintf (output_file,
					 "\t{\"%s\", &%s, (void *) field_offset (%s, %s)},\n",
					 field.name, [field_type cexprType], [self outname],
					 field.name);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");

	fprintf (output_file, "static exprtab_t %s_symtab = {\n", [self outname]);
	fprintf (output_file, "\t%s_symbols,\n", [self outname]);
	fprintf (output_file, "};\n");

	fprintf (output_file, "exprtype_t %s_type = {\n", [self outname]);
	fprintf (output_file, "\t.name = \"%s\",\n", [self outname]);
	fprintf (output_file, "\t.size = sizeof (%s),\n", [self outname]);
	fprintf (output_file, "\t.binops = cexpr_struct_binops,\n");
	fprintf (output_file, "\t.unops = 0,\n");
	fprintf (output_file, "\t.data = &%s_symtab,\n", [self outname]);
	fprintf (output_file, "};\n");
	fprintf (output_file, "\n");
	fprintf (header_file, "extern exprtype_t %s_type;\n", [self outname]);
}

-(void) writeTable
{
	if ([parse string] == "skip") {
		return;
	}

	PLItem     *field_dict = [parse getObjectForKey:[self name]];

	PLItem     *new_name = [field_dict getObjectForKey:".name"];
	if (new_name) {
		outname = str_hold ([new_name string]);
	}

	Array      *field_defs = [Array array];
	PLItem     *only = [field_dict getObjectForKey:".only"];
	if (only) {
		string      field_name = [only string];
		qfot_var_t *field = nil;
		for (int i = 0; i < type.strct.num_fields; i++) {
			qfot_var_t *f = &type.strct.fields[i];
			if (f.name == field_name) {
				field = f;
				break;
			}
		}
		Type       *field_type = [Type findType: field.type];
		FieldDef   *field_def = [field_type fielddef:self field:field.name];
		if (!field_def) {
			field_def = [FieldDef fielddef:nil
									struct:self
									 field:field.name];
		}
		[field_defs addObject: field_def];
	} else if (field_dict) {
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
			Type       *field_type = [Type findType: field.type];
			FieldDef   *field_def = [field_type fielddef:self field:field.name];
			if (!field_def) {
				field_def = [FieldDef fielddef:nil
										struct:self
										 field:field.name];
			}
			[field_defs addObject: field_def];
		}
	}

	if ([field_dict getObjectForKey:".type"]) {
		PLItem     *type = [field_dict getObjectForKey:".type"];
		string      str = [type string];

		write_function_head (self);
		fprintf (output_file, "\tpltype_t    type = PL_Type (item);\n");
		if (str) {
			write_type (self, field_dict, str);
		} else {
			for (int i = [type count]; i-- > 0; ) {
				str = [[type getObjectAtIndex:i] string];
				write_type (self, field_dict, str);
			}
		}
		fprintf (output_file,
				 "\tPL_TypeMismatch (messages, item, field->name, %s, type);\n",
				 parseItemType (type));
		fprintf (output_file, "\treturn 0;\n");
		write_function_tail (self);
		write_cexpr (self, field_defs);
		return;
	}

	int         have_sType = 0;
	int         have_pNext = 0;
	int         readonly = [field_dict string] == "readonly";

	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *field = &type.strct.fields[i];
		if (field.name == "sType") {
			have_sType = 1;
		}
		if (field.name == "pNext") {
			have_pNext = 1;
			write_symtab = 1;
		}
	}
	for (int i = [field_defs count]; i-- > 0; ) {
		FieldDef   *field_def = [field_defs objectAtIndex:i];
		[field_def writeParseData];
	}
	if (!readonly) {
		fprintf (output_file, "static plfield_t %s_fields[] = {\n",
				 [self outname]);
		if (!only) {
			fprintf (output_file,
					 "\t{\"@inherit\", 0, QFString, parse_inherit, "
					 "&%s_fields},\n", [self outname]);
		}
		if (have_pNext) {
			fprintf (output_file,
					"\t{\"@next\", field_offset (%s, pNext), "
					"QFArray, parse_next, 0},", [self outname]);
		}
		for (int i = [field_defs count]; i-- > 0; ) {
			FieldDef   *field_def = [field_defs objectAtIndex:i];
			[field_def writeField];
		}
		fprintf (output_file, "\t{ }\n");
		fprintf (output_file, "};\n");

		write_function_head (self);
		if (have_sType) {
			fprintf (output_file, "\t((%s *) data)->sType", [self outname]);
			fprintf (output_file, " = %s;\n", [self sTypeName]);
		}
		if (label_field) {
			fprintf (output_file, "\t((%s *) data)->%s", [self outname],
					 label_field);
			fprintf (output_file, " = vkstrdup (context, field->name);\n");
		}
		if (only) {
			fprintf (output_file, "\tplfield_t  *f = &%s_fields[0];\n",
					 [self outname]);
			fprintf (output_file,
					 "\tif (!PL_CheckType (PL_Type (item), f->type)) {\n"
					 "\t\tPL_TypeMismatch (messages, item, "
					 "f->name, f->type, PL_Type (item));\n"
					 "\t\treturn 0;\n"
					 "\t}\n"
					 "\tvoid       *flddata = (byte *)data + f->offset;\n"
					 "\treturn f->parser (f, item, flddata, messages, "
					 "context);\n");
		} else {
			fprintf (output_file,
					 "\tif (PL_Type (item) == QFString\n"
					 "\t\t&& !(item = parse_reference (item, \"%s\", "
					 "messages, context))) {\n"
					 "\t\treturn 0;\n"
					 "\t}\n"
					 "\treturn PL_ParseStruct (%s_fields, item, data, "
					 "messages, context);\n",
					 [self outname], [self outname]);
		}
		write_function_tail (self);
		if (have_pNext) {
			fprintf (output_file, "static parserref_t %s_parser = ",
					 [self outname]);
			fprintf (output_file, "{\"%s\", %s, sizeof(%s)};\n",
					 [self outname], [self parseFunc], [self outname]);
		}
	}

	write_cexpr (self, field_defs);
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

-(void) writeSymtabEntry
{
	if (!write_symtab || [parse string] == "skip") {
		return;
	}
	fprintf (output_file,
			 "\tHash_Add (parser_table, &%s_parser);\n",
			 [self outname]);
}

-(string) outname
{
	if (outname) {
		return outname;
	}
	if (alias) {
		return [alias name];
	}
	return [self name];
}

-(string) cexprType
{
	return [self outname] + "_type";
}

-(string) parseType
{
	return "QFMultiType | (1 << QFString) | (1 << QFDictionary)";
}

-(string) parseFunc
{
	return "parse_" + [self outname];
}

-(string) parseData
{
	return "0";
}
@end
