#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>
#include <PropertyList.h>

#include "vkfielddef.h"
#include "vkgen.h"
#include "vkfixedarray.h"

@implementation FixedArray

-initWithType: (qfot_type_t *) type
{
	if (!(self = [super initWithType: type])) {
		return nil;
	}
	ele_type = [Type fromType: type.array.type];
	ele_count = type.array.count;
	return self;
}

-(string) name
{
	return sprintf ("%s_array_%d", [ele_type name], ele_count);
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

-(void) writeForward
{
}

-(void) writeTable
{
	fprintf (output_file, "static parse_fixed_array_t parse_%s_data = {\n",
			 [self name]);
	fprintf (output_file, "\t.type = %s,\n", [ele_type parseType]);
	fprintf (output_file, "\t.stride = sizeof (%s),\n", [ele_type name]);
	fprintf (output_file, "\t.parser = %s,\n", [ele_type parseFunc]);
	fprintf (output_file, "\t.size = %d,\n", ele_count);
	fprintf (output_file, "};\n");

	fprintf (output_file, "exprarray_t %s_array = {\n", [self name]);
	fprintf (output_file, "\t.type = &%s,\n", [ele_type cexprType]);
	fprintf (output_file, "\t.size = %d,\n", ele_count);
	fprintf (output_file, "};\n");
	fprintf (output_file, "exprtype_t %s_type = {\n", [self name]);
	fprintf (output_file, "\t.name = \"%s[%d]\",\n", [ele_type name],
			 ele_count);
	fprintf (output_file, "\t.size = %d * sizeof (%s),\n", ele_count,
			 [ele_type name]);
	fprintf (output_file, "\t.binops = cexpr_array_binops,\n");
	fprintf (output_file, "\t.unops = 0,\n");
	fprintf (output_file, "\t.data = &%s_array,\n", [self name]);
	fprintf (output_file, "};\n");
	fprintf (output_file, "\n");
	fprintf (header_file, "extern exprtype_t %s_type;\n", [self name]);
}

-(void) writeSymtabInit
{
}

-(void) writeSymtabShutdown
{
}

-(void) writeSymtabEntry
{
}

-(string) cexprType
{
	return [self name] + "_type";
}

-(string) parseType
{
	return "QFMultiType | (1 << QFString) | (1 << QFArray)";
}

-(string) parseFunc
{
	return "parse_fixed_array";
}

-(string) parseData
{
	return "&parse_" + [self name] + "_data";;
}
@end
