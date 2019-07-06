#ifndef __QF_Vulkan_funcs_h
#define __QF_Vulkan_funcs_h

#define EXPORTED_VULKAN_FUNCTION(fname) extern PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) extern PFN_##fname fname;

#include "QF/Vulkan/funclist.h"

#endif//__QF_Vulkan_funcs_h
