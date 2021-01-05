#include <string.h>
#include <PropertyList.h>

#include "vkgen.h"
#include "vkhandle.h"

void
output_handle (string name, PLItem *handle)
{
	string symtab = str_hold ([[handle getObjectForKey:"symtab"] string]);
	string class = str_hold ([[handle getObjectForKey:"class"] string]);
	string create = str_hold ([[handle getObjectForKey:"create"] string]);
	string custom = str_hold ([[handle getObjectForKey:"custom"] string]);
	if (!custom) {
		fprintf (output_file, "static int parse_%s (const plitem_t *item, void **data, plitem_t *messages, parsectx_t *context)\n", name);
		fprintf (output_file, "{\n");
		fprintf (output_file, "\t__auto_type handle = (%s *) data[0];\n", name);
		fprintf (output_file, "\tvulkan_ctx_t *ctx = context->vctx;\n");
		fprintf (output_file, "\tqfv_device_t *device = ctx->device;\n");
		fprintf (output_file, "\tqfv_devfuncs_t *dfunc = device->funcs;\n");
		fprintf (output_file, "\tif (PL_Type (item) == QFString) {\n");
		fprintf (output_file, "\t\tconst char *name = PL_String (item);\n");
		fprintf (output_file, "\t\thandleref_t *hr = Hash_Find (ctx->%s, name);\n", symtab);
		fprintf (output_file, "\t\tif (!hr) {\n");
		fprintf (output_file, "\t\t\tPL_Message (messages, item, \"undefined %s %%s\", name);\n", class);
		fprintf (output_file, "\t\t\treturn 0;\n");
		fprintf (output_file, "\t\t}\n");
		fprintf (output_file, "\t\t*handle = (%s) hr->handle;\n", name);
		fprintf (output_file, "\t\treturn 1;\n");
		fprintf (output_file, "\t}\n");

		fprintf (output_file, "\t%sCreateInfo createInfo = {};\n", name);

		fprintf (output_file, "\tif (!parse_%sCreateInfo (0, item, &createInfo, messages, context)) {\n", name);
		fprintf (output_file, "\t\treturn 0;\n");
		fprintf (output_file, "\t}\n");
		fprintf (output_file, "\tVkResult res;\n");
		fprintf (output_file, "\tres = dfunc->%s (device->dev, &createInfo, 0, handle);\n", create);
		fprintf (output_file, "\tif (res != VK_SUCCESS) {\n");
		fprintf (output_file, "\t\tPL_Message (messages, item, \"could not create %s\");\n", class);
		fprintf (output_file, "\t\treturn 0;\n");
		fprintf (output_file, "\t}\n");
		fprintf (output_file, "\treturn 1;\n");
		fprintf (output_file, "}\n");
	}

	fprintf (output_file, "int parse_%s_handleref (const plfield_t *field, const plitem_t *item, void *data, plitem_t *messages, void *context)\n", name);
	fprintf (output_file, "{\n");
	fprintf (output_file, "\thandleref_t *handleref = data;\n");
	fprintf (output_file, "\tvoid *hrdata[] = { &handleref->handle };\n");
	fprintf (output_file, "\thandleref->name = strdup (field->name);\n");
	if (custom) {
		fprintf (output_file, "\treturn %s (item, hrdata, messages, context);\n", custom);
	} else {
		fprintf (output_file, "\treturn parse_%s (item, hrdata, messages, context);\n", name);
	}
	fprintf (output_file, "}\n");

	if (!custom) {
		fprintf (header_file, "static int parse_%s (const plitem_t *item, void **data, plitem_t *messages, parsectx_t *context);\n", name);
	}
	fprintf (header_file, "int parse_%s_handleref (const plfield_t *field, const plitem_t *item, void *data, plitem_t *messages, void *context);\n", name);
	str_free (custom);
	str_free (symtab);
	str_free (class);
	str_free (create);
}
