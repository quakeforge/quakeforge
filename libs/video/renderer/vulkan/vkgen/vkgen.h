#ifndef __renderer_vulkan_vkgen_vkgen_h
#define __renderer_vulkan_vkgen_vkgen_h

#include <hash.h>
#include <qfile.h>
#include <types.h>
#include <Array.h>
#include <PropertyList.h>

typedef void varfunc (qfot_var_t *var);

void printf (string fmt, ...);
void fprintf (QFile file, string format, ...);
extern Array *queue;
extern Array *output_types;
extern PLItem *parse;
extern QFile output_file;
extern QFile header_file;
extern hashtab_t *processed_types;
extern hashtab_t *available_types;

#endif//__renderer_vulkan_vkgen_vkgen_h
