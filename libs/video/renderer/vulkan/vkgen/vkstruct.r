#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>

#include "vkgen.h"
#include "vkstruct.h"

@implementation Struct

-(string) name
{
	return str_mid(type.strct.tag, 4);
}

-(void) forEachFieldCall: (varfunc) func
{
	qfot_struct_t *strct =&type.strct;

	for (int i = 0; i < strct.num_fields; i++) {
		func (&strct.fields[i]);
	}
}

-(void) writeTable
{
	fprintf (output_file, "structfld_t %s_fields[] = {\n", [self name]);
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (var.name == "sType" || var.name == "pNext") {
			continue;
		}
		string type_name = var.type.encoding;
		fprintf (output_file,
				 "\t{\"%s\", field_offset (%s, %s), %s, %s}, // %s\n",
				 var.name, [self name], var.name, "0", "0", type_name);
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");
}
@end
