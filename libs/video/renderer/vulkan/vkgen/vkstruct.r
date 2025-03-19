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
	[field_dict release];
	[field_defs release];
	[parse_def release];
	[only release];
	str_free (outname);
	str_free (label_field);
	[super dealloc];
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
	bool        readonly = [field_dict string] == "readonly";

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
	bool        readonly = [field_dict string] == "readonly";
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
write_parse_type (Struct *self, PLItem *item)
{
	if ([item string] == "auto") {
		fprintf (output_file, "\t\tif (!PL_ParseStruct (%s_fields, item, "
				 "data, messages, context)) {\n", [self outname]);
		fprintf (output_file, "\t\t\treturn 0;\n");
		fprintf (output_file, "\t\t}\n");
	} else {
		//FieldDef   *def = [FieldDef fielddef:item struct:self field:".parse"];
		[self.parse_def writeParse];
	}
}

static void
write_auto_parse (Struct *self, string field, int name)
{
	string      item = name ? "name" : "item";

	fprintf (output_file, "\t\tdo {\n");
	fprintf (output_file, "\t\t\tplfield_t  *f = find_field (%s_fields, %s, "
			 "item, messages);\n", [self outname], sprintf ("\"%s\"", field));
	fprintf (output_file, "\t\t\tif (!f) {\n");
	fprintf (output_file, "\t\t\t\treturn 0;\n");
	fprintf (output_file, "\t\t\t}\n");
	if (name) {
		fprintf (output_file, "\t\t\tplitem_t   *name = "
				 "PL_NewString (field->name);\n");
	}
	fprintf (output_file, "\t\t\tf->parser (f, %s, &%s, messages, context);\n",
			 item, sprintf ("((%s *) data)->%s", [self outname], field));
	if (name) {
		fprintf (output_file, "\t\t\tPL_Release (name);\n");
	}
	fprintf (output_file, "\t\t} while (0);\n");
}

static int
check_need_table (Struct *self, PLItem *field_dict, string type)
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
		return 0;
	}
	for (int i = 0; i < count; i++) {
		string      field = [type_obj keyAtIndex:i];
		PLItem     *item = [type_obj getObjectForKey:field];
		string      str = [item string];

		if (field == ".parse") {
			if (str != "auto") {
				self.parse_def = [[FieldDef fielddef:item
											 struct:self
											  field:"parse"] retain];
			}
			return 1;
		}
		if (str == "$auto") {
			return 1;
		}
	}
	return 0;
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

		if (field == ".parse") {
			write_parse_type (self, item);
			continue;
		}

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
			case "$auto":
				write_auto_parse (self, field, 0);
				continue;
			case "$name.auto":
				write_auto_parse (self, field, 1);
				continue;
		}
		fprintf (output_file, "\t\t((%s *) data)->%s = %s;\n", [self outname],
				 field, str);
	}
	fprintf (output_file, "\t\treturn 1;\n");
	fprintf (output_file, "\t}\n");
}

static void
write_parser (Struct *self, bool have_sType, PLItem *only)
{
	write_function_head (self);
	if (have_sType) {
		fprintf (output_file, "\t((%s *) data)->sType", [self outname]);
		fprintf (output_file, " = %s;\n", [self sTypeName]);
	}
	if (self.label_field) {
		fprintf (output_file, "\t((%s *) data)->%s", [self outname],
				 self.label_field);
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
					 "\t{\"%s\", &%s, (void *) offsetof (%s, %s)},\n",
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

static void
write_table (Struct *self, PLItem *field_dict, Array *field_defs,
			 PLItem *only, int need_parser)
{
	qfot_type_t *type = self.type;
	bool        have_sType = false;
	bool        have_pNext = false;
	bool        readonly = [field_dict string] == "readonly";

	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *field = &type.strct.fields[i];
		if (field.name == "sType") {
			have_sType = true;
		}
		if (field.name == "pNext") {
			have_pNext = true;
			self.write_symtab = 1;
		}
	}

	for (int i = [field_defs count]; i-- > 0; ) {
		FieldDef   *field_def = [field_defs objectAtIndex:i];
		[field_def writeParseData];
	}
	[self.parse_def writeParseData];

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
					"\t{\"@next\", offsetof (%s, pNext), "
					"QFArray, parse_next, 0},\n", [self outname]);
		}
		for (int i = [field_defs count]; i-- > 0; ) {
			FieldDef   *field_def = [field_defs objectAtIndex:i];
			[field_def writeField];
		}
		fprintf (output_file, "\t{ }\n");
		fprintf (output_file, "};\n");

		if (need_parser) {
			write_parser (self, have_sType, only);
		}
		if (have_pNext) {
			fprintf (output_file, "static parserref_t %s_parser = ",
					 [self outname]);
			fprintf (output_file, "{\"%s\", %s, sizeof(%s)};\n",
					 [self outname], [self parseFunc], [self outname]);
		}
	}
}

-(void) initParse:(PLItem *)parse
{
	if ([parse string] == "skip") {
		skip = 1;
		return;
	}

	field_dict = [parse retain];

	PLItem     *new_name = [field_dict getObjectForKey:".name"];
	if (new_name) {
		outname = str_hold ([new_name string]);
	}

	field_defs = [[Array array] retain];

	only = [[field_dict getObjectForKey:".only"] retain];
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
}

-(void) writeTable
{
	if (skip) {
		return;
	}

	if ([field_dict getObjectForKey:".type"]) {
		PLItem     *type = [field_dict getObjectForKey:".type"];
		string      str = [type string];
		int         need_table = 0;

		if (str) {
			need_table |= check_need_table (self, field_dict, str);
		} else {
			for (int i = [type count]; i-- > 0; ) {
				string      str = [[type getObjectAtIndex:i] string];
				need_table |= check_need_table (self, field_dict, str);
			}
		}
		if (need_table) {
			write_table (self, field_dict, field_defs, only, 0);
		}
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

	write_table (self, field_dict, field_defs, only, 1);

	write_cexpr (self, field_defs);
}

-(void) writeSymtabInit
{
	PLItem     *field_dict = [parse getObjectForKey:[self outname]];

	if ([parse string] == "skip") {
		return;
	}

	fprintf (output_file, "\tqfMessageL (\"%s_symtab\");\n", [self outname]);
	fprintf (output_file, "\tcexpr_init_symtab (&%s_symtab, context);\n",
			 [self outname]);
}

-(void) writeSymtabShutdown
{
	PLItem     *field_dict = [parse getObjectForKey:[self outname]];

	if ([parse string] == "skip") {
		return;
	}

	fprintf (output_file, "\tHash_DelTable (%s_symtab.tab);\n",
			 [self outname]);
	fprintf (output_file, "\t%s_symtab.tab = nullptr;\n", [self outname]);
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
