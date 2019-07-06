#include <vulkan/vulkan.h>

#define EXPORTED_VULKAN_FUNCTION(fname) PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) PFN_##fname fname;

#include "QF/Vulkan/funclist.h"
