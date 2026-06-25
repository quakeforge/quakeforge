#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dlfcn.h>

#include "QF/hash.h"
#include "QF/plist.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/swapchain.h"
#include "QF/Vulkan/qf_vid.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#include "../vkparse.h"

static const char *vulkan_library_name = "libvulkan.so.1";
static void *vulkan_library;

static void
load_vulkan_library (vulkan_ctx_t *ctx)
{
	vulkan_library = dlopen (vulkan_library_name, RTLD_NOW);
	if (!vulkan_library) {
		Sys_Error ("Couldn't load vulkan library %s: %s",
				   vulkan_library_name, dlerror ());
	}

	#define EXPORTED_VULKAN_FUNCTION(name) \
	ctx->name = (PFN_##name) dlsym (vulkan_library, #name); \
	if (!ctx->name) { \
		Sys_Error ("Couldn't find exported vulkan function %s", #name); \
	}

	#define GLOBAL_LEVEL_VULKAN_FUNCTION(name) \
	ctx->name = (PFN_##name) ctx->vkGetInstanceProcAddr (0, #name); \
	if (!ctx->name) { \
		Sys_Error ("Couldn't find global-level function %s", #name); \
	}

	#include "QF/Vulkan/funclist.h"
}

static void
unload_vulkan_library (vulkan_ctx_t *ctx)
{
	dlclose (vulkan_library);
	vulkan_library = 0;
}

static int
no_presentation (vulkan_ctx_t *ctx, VkPhysicalDevice physicalDevice,
				 uint32_t queueFamilyIndex)
{
	return 1;
}

static vulkan_ctx_t *ctx;

//FIXME these should not be here
int scr_fisheye;
struct vid_render_funcs_s *r_funcs;
void R_Init_Cvars () {};

static const char graph_plist[] = {
#embed "prefixsum.plist" suffix(,)
	0
};

static uint32_t *prefixsum;
static uint32_t *sums;
static uint32_t *expect_prefixsum;
static uint32_t *expect_sums;

static void
initialize_base_data (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	auto count = *(uint32_t *) params[0]->value;
	auto buff_name = *(const char **) params[1]->value;
	auto info = QFV_FindBufferInfo (ctx, buff_name);
	auto buffer = QFV_GetBuffer (ctx, info);

	size_t packet_size = count * sizeof (uint32_t);
	auto packet = QFV_PacketAcquire (ctx->staging, "prefixsum.init");
	uint32_t *packet_start = QFV_PacketExtend (packet, packet_size);
	prefixsum = malloc (packet_size);
	expect_prefixsum = malloc (packet_size);
	sums = malloc (packet_size / 1024);
	expect_sums = malloc (packet_size / 1024);

	for (uint32_t i = 0; i < count; i++) {
		uint32_t val = i ^ (i >> 1);	// gray scale
		expect_prefixsum[i] = val;
		packet_start[i] = val;
	}
	for (uint32_t i = 0; i < count / 1024; i++) {
		uint32_t sum = 0;
		for (uint32_t j = 0; j < 1024; j++) {
			uint32_t ind = i * 1024 + j;
			uint32_t val = expect_prefixsum[ind];
			expect_prefixsum[ind] = sum;
			sum += val;
		}
		expect_sums[i] = sum;
	}

	auto srcBarrier = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	auto dstBarrier = bufferBarriers[qfv_BB_TransferWrite_to_ShaderRW];
	QFV_PacketCopyBuffer (packet, buffer, 0, &srcBarrier, &dstBarrier);

	QFV_PacketSubmit (packet);
}

static void
copy_data (vulkan_ctx_t *ctx, VkBuffer dst, VkBuffer src, size_t count)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto queue = device->queue.queue;

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	dfunc->vkBeginCommandBuffer (cmd, &(VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	auto bb = bufferBarriers[qfv_BB_ShaderWrite_to_TransferRead];
	VkMemoryBarrier2 mb = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = bb.srcStageMask,
		.srcAccessMask = bb.srcAccessMask,
		.dstStageMask = bb.dstStageMask,
		.dstAccessMask = bb.dstAccessMask,
	};
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mb,
	};

	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);

	dfunc->vkCmdCopyBuffer (cmd, src, dst, 1, &(VkBufferCopy) {
		.size = count,
	});
	dfunc->vkEndCommandBuffer (cmd);

	auto fence = QFV_CreateFence (device, 0);
	dfunc->vkQueueSubmit (queue, 1, &(VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	}, fence);
	dfunc->vkWaitForFences (device->dev, 1, &fence, VK_TRUE, 2000000000);
	dfunc->vkDestroyFence (device->dev, fence, 0);
}

static void
readback_prefixsum (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto count = *(uint32_t *) params[0]->value;
	auto sum_buff_name = *(const char **) params[1]->value;
	auto out_buff_name = *(const char **) params[2]->value;
	auto out_info = QFV_FindBufferInfo (ctx, out_buff_name);
	auto sum_info = QFV_FindBufferInfo (ctx, sum_buff_name);
	auto out_buffer = QFV_GetBuffer (ctx, out_info);
	auto sum_buffer = QFV_GetBuffer (ctx, sum_info);

	size_t packet_size = count * sizeof (uint32_t);
	qfv_resobj_t buffer_obj = {
		.name = "buffer",
		.type = qfv_res_buffer,
		.buffer = {
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			.size = packet_size,
		},
	};
	qfv_resource_t resource = {
		.name = "readback",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = 1,
		.objects = &buffer_obj,
	};
	QFV_CreateResource (device, &resource);
	VkBuffer readback_buffer = buffer_obj.buffer.buffer;
	void *readback;
	dfunc->vkMapMemory (device->dev, resource.memory, 0, resource.size, 0,
						&readback);

	copy_data (ctx, readback_buffer, out_buffer, packet_size);
	memcpy (prefixsum, readback, packet_size);
	copy_data (ctx, readback_buffer, sum_buffer, packet_size / 1024);
	memcpy (sums, readback, packet_size / 1024);

	QFV_DestroyResource (device, &resource);
}

static exprtype_t *initialize_base_data_params[] = {
	&cexpr_uint,
	&cexpr_string,
};
static exprfunc_t initialize_base_data_func[] = {
	{ .func = initialize_base_data,
		.num_params = countof (initialize_base_data_params),
		.param_types = initialize_base_data_params },
	{}
};

static exprtype_t *readback_prefixsum_params[] = {
	&cexpr_uint,
	&cexpr_string,
	&cexpr_string,
};
static exprfunc_t readback_prefixsum_func[] = {
	{ .func = readback_prefixsum,
		.num_params = countof (readback_prefixsum_params),
		.param_types = readback_prefixsum_params },
	{}
};

static exprsym_t prefixsum_task_syms[] = {
	{ "initialize_base_data", &cexpr_function, initialize_base_data_func },
	{ "readback_prefixsum", &cexpr_function, readback_prefixsum_func },
	{}
};

static void
print_time (const char *prefix, const char *name, qfv_time_t time)
{
	printf ("%s%s: %7"PRIu64"\u03bcs\n", prefix, name, time.cur_time);
}

int
main ()
{
	size_t      memsize = 128 * 1024 * 1024;
	hashctx_t  *hashctx = nullptr;

	vulkan_use_validation = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	qfv_swapchain_t swapchain = {};

	ctx = malloc (sizeof (vulkan_ctx_t));
	*ctx = (vulkan_ctx_t) {
		.load_vulkan = load_vulkan_library,
		.unload_vulkan = unload_vulkan_library,
		.get_presentation_support = no_presentation,
		.hunk = Hunk_Init (Sys_Alloc (memsize), memsize),
		.va_ctx = va_create_context (VA_CTX_COUNT),
		.swapchain = &swapchain,
	};

	plitem_t *config = PL_GetPropertyList (graph_plist, &hashctx);

	ctx->load_vulkan (ctx);
	Vulkan_Script_Init (ctx);
	ctx->instance = QFV_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff,
										nullptr, nullptr);
	ctx->device = QFV_CreateDevice (ctx, nullptr);
	ctx->cmdpool = QFV_CreateCommandPool (ctx->device,
										  ctx->device->queue.queueFamily,
										  0, 1, "vulkan_ctx");

	Vulkan_CreateStagingBuffers (ctx);

	QFV_Render_Init (ctx);
	QFV_Render_AddTasks (ctx, prefixsum_task_syms);

	QFV_LoadRenderInfo (ctx, config);
	QFV_BuildRender (ctx, nullptr);

	auto graph = ctx->render_context->graph;
	auto job = QFV_FindJob ("prefixsum", graph);
	QFV_RunRenderJob (ctx, job);

	print_time ("", job->label.name, job->time);
	for (uint32_t i = 0; i < job->num_steps; i++) {
		auto step = &job->steps[i];
		print_time ("    ", step->label.name, step->time);
	}

	QFV_Render_Shutdown (ctx);
	QFV_DestroyStagingBuffer (ctx->staging);
	auto dev = ctx->device->dev;
	auto df = ctx->device->funcs;
	df->vkDestroyCommandPool (dev, ctx->cmdpool, 0);
	Vulkan_Script_Shutdown (ctx);
	if (ctx->device) {
		QFV_DestroyDevice (ctx->device);
	}
	if (ctx->instance) {
		QFV_DestroyInstance (ctx->instance);
	}

	va_destroy_context (ctx->va_ctx);
	free (ctx);

	for (uint32_t i = 0; i < 1024*1024; i++) {
		if (expect_prefixsum[i] != prefixsum[i]) {
			printf ("prefixsum differs at %u: %u %u\n", i,
					expect_prefixsum[i], prefixsum[i]);
			return 1;
		};
	}
	for (uint32_t i = 0; i < 1024; i++) {
		if (expect_sums[i] != sums[i]) {
			printf ("sums differs at %u: %u %u\n", i,
					expect_sums[i], sums[i]);
			return 1;
		};
	}
	return 0;
}
