#include <string.h>
#include <PropertyList.h>

#include "vkgen.h"
#include "vkresource.h"

void
output_resource_data (PLItem *resource)
{
	string name = str_hold ([[resource getObjectForKey:"name"] string]);
	string parse_type = str_hold ([[resource getObjectForKey:"parse_type"] string]);
	string parser = str_hold ([[resource getObjectForKey:"parser"] string]);
	string type = str_hold ([[resource getObjectForKey:"type"] string]);

	fprintf (output_file, "static plelement_t resource_%s_data = {\n", name);
	fprintf (output_file, "\t%s,\n", parse_type);
	fprintf (output_file, "\tsizeof (%s),\n", type);
	fprintf (output_file, "\tmalloc,\n");
	fprintf (output_file, "\t%s,\n", parser);
	fprintf (output_file, "\t0,\n");
	fprintf (output_file, "};\n");

	fprintf (output_file, "static plfield_t resource_%s_field = {\n", name);
	fprintf (output_file, "\t0, 0, %s, 0, &resource_%s_data\n",
			 parse_type, name);
	fprintf (output_file, "};\n");

	str_free (name);
	str_free (parse_type);
	str_free (parser);
	str_free (type);
}

void
output_resource_entry (PLItem *resource)
{
	string name = str_hold ([[resource getObjectForKey:"name"] string]);
	string table = str_hold ([[resource getObjectForKey:"table"] string]);
	fprintf (output_file,
		"\t{\"%s\", &resource_%s_field, field_offset (vulkan_ctx_t, %s) },\n",
		 name, name, table);
	str_free (name);
	str_free (table);
}
