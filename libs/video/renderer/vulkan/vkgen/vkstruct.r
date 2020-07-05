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
	PLItem     *field_dict = [parse getObjectForKey:[self name]];
	PLItem     *field_defs = [field_dict allKeys];
	Type       *field_type;

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

			if (!type_desc) {
				continue;
			}
			type_record = [[type_desc getObjectAtIndex:0] string];
			type_type = [[type_desc getObjectAtIndex:1] string];

			field_type = [[Type lookup: type_type] dereference];
			fprintf (output_file, "static parse_%s_t parse_%s_%s_data = {\n",
					 type_record, [self name], field_name);
			fprintf (output_file, "\tparse_%s,\n", type_type);
			if (type_record == "single") {
				value_field = [[field_def getObjectForKey:"value"] string];
				fprintf (output_file, "\tfield_offset (%s, %s),\n",
						 [self name], value_field);
			} else {
				value_field = [[field_def getObjectForKey:"values"] string];
				size_field = [[field_def getObjectForKey:"size"] string];
				fprintf (output_file, "\tfield_offset (%s, %s),\n",
						 [self name], value_field);
				if (size_field) {
					fprintf (output_file, "\tfield_offset (%s, %s),\n",
							 [self name], size_field);
				} else {
					fprintf (output_file, "\t-1,\n");
				}
			}
			fprintf (output_file, "};\n");
		}
	}
	fprintf (output_file, "static plfield_t %s_fields[] = {\n", [self name]);
	if (field_defs) {
		PLItem     *field_def;
		qfot_var_t *field;

		for (int i = [field_defs count]; i-- > 0; ) {
			string field_name = [[field_defs  getObjectAtIndex:i] string];
			field_def = [field_dict getObjectForKey:field_name];
			if ([field_def string] == "auto") {
				field = [self findField:field_name];
				if (!field) {
					continue;
				}
				field_type = [Type findType: field.type];
				fprintf (output_file,
						 "\t{\"%s\", field_offset (%s, %s), %s, %s, %s},\n",
						 field_name, [self name], field_name,
						 [field_type parseType], [field_type parseFunc],
						 [field_type parseData]);
			} else {
				PLItem     *type_desc = [field_def getObjectForKey:"type"];
				string      type_record;
				string      type_type;
				string      parseType;

				type_record = [[type_desc getObjectAtIndex:0] string];
				type_type = [[type_desc getObjectAtIndex:1] string];

				field_type = [[Type lookup: type_type] dereference];
				if (type_record == "single") {
					parseType = [field_type parseType];
				} else {
					parseType = "QFArray";
				}
				fprintf (output_file,
						 "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
						 field_name, parseType, type_record,
						 [self name], field_name);
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
					 field.name, [self name], field.name,
					 [field_type parseType], [field_type parseFunc],
					 [field_type parseData]);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");

	fprintf (output_file, "static int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages)\n",
			 [self name]);
	fprintf (output_file, "{\n");
	fprintf (output_file,
			 "\treturn PL_ParseDictionary (%s_fields, item, data, messages);\n",
			 [self name]);
	fprintf (output_file, "}\n");
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
