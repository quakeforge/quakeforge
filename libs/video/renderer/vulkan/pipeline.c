/*
	pipeline.c

	Vulkan pipeline functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2020      Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/renderpass.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

qfv_shadermodule_t *
QFV_CreateShaderModule (qfv_device_t *device,
						size_t size, const uint32_t *code)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkShaderModuleCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 0, 0,
		size, code,
	};

	qfv_shadermodule_t *module = malloc (sizeof (*module));
	module->device = device;
	dfunc->vkCreateShaderModule (dev, &createInfo, 0, &module->module);
	return module;
}

void
QFV_DestroyShaderModule (qfv_shadermodule_t *module)
{
	qfv_device_t *device = module->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyShaderModule (dev, module->module, 0);
	free (module);
}

qfv_pipelinecache_t *
QFV_CreatePipelineCache (qfv_device_t *device, dstring_t *cacheData)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkPipelineCacheCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, 0, 0,
		cacheData->size, cacheData->str,
	};

	qfv_pipelinecache_t *cache = malloc (sizeof (*cache));
	cache->device = device;
	dfunc->vkCreatePipelineCache (dev, &createInfo, 0, &cache->cache);
	return cache;
}

dstring_t *
QFV_GetPipelineCacheData (qfv_pipelinecache_t *cache)
{
	qfv_device_t *device = cache->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	dstring_t  *cacheData = dstring_new ();

	dfunc->vkGetPipelineCacheData (dev, cache->cache, &cacheData->size, 0);
	dstring_adjust (cacheData);
	dfunc->vkGetPipelineCacheData (dev, cache->cache, &cacheData->size,
								   cacheData->str);
	return cacheData;
}

void
QFV_MergePipelineCaches (qfv_pipelinecache_t *targetCache,
						 qfv_pipelinecacheset_t *sourceCaches)
{
	qfv_device_t *device = targetCache->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkPipelineCache *srcCaches = alloca (sourceCaches->numCaches
										 * sizeof (*srcCaches));
	for (uint32_t i = 0; i < sourceCaches->numCaches; i++) {
		srcCaches[i] = sourceCaches->caches[i]->cache;
	}
	dfunc->vkMergePipelineCaches (dev, targetCache->cache,
								  sourceCaches->numCaches, srcCaches);
}

void
QFV_DestroyPipelineCache (qfv_pipelinecache_t *cache)
{
	qfv_device_t *device = cache->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyPipelineCache (dev, cache->cache, 0);
}

qfv_pipelinelayout_t *
QFV_CreatePipelineLayout (qfv_device_t *device,
						  qfv_descriptorsetlayoutset_t *layouts,
                          qfv_pushconstantrangeset_t *pushConstants)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 0, 0,
		layouts->size, layouts->a,
		pushConstants->numRanges, pushConstants->ranges,
	};

	qfv_pipelinelayout_t *layout = malloc (sizeof (layout));
	layout->device = device;
	dfunc->vkCreatePipelineLayout (dev, &createInfo, 0, &layout->layout);
	return layout;
}

void
QFV_DestroyPipelineLayout (qfv_pipelinelayout_t *layout)
{
	qfv_device_t *device = layout->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyPipelineLayout (dev, layout->layout, 0);
	free (layout);
}

static VkPipelineShaderStageCreateInfo
shaderStageCI (qfv_shaderstageparams_t params)
{
	VkPipelineShaderStageCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0,
		params.stageFlags, params.module->module,
		params.entryPointName, params.specializationInfo,
	};
	return createInfo;
}

static VkPipelineVertexInputStateCreateInfo
vertexInputSCI (qfv_vertexinputstate_t vertexState)
{
	VkPipelineVertexInputStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, 0, 0,
		vertexState.bindings->size,
		vertexState.bindings->a,
		vertexState.attributes->size,
		vertexState.attributes->a,
	};
	return createInfo;
}

static VkPipelineInputAssemblyStateCreateInfo
inputAssemblySCI (qfv_pipelineinputassembly_t inputAssemblyState)
{
	VkPipelineInputAssemblyStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, 0, 0,
		inputAssemblyState.topology,
		inputAssemblyState.primativeRestartEnable,
	};
	return createInfo;
}

static VkPipelineTessellationStateCreateInfo
tessellationSCI (qfv_pipelinetessellation_t tessellationState)
{
	VkPipelineTessellationStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, 0, 0,
		tessellationState.patchControlPoints
	};
	return createInfo;
}

static VkPipelineViewportStateCreateInfo
viewportSCI (qfv_viewportinfo_t viewportState)
{
	VkPipelineViewportStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, 0, 0,
		viewportState.viewportset->numViewports,
		viewportState.viewportset->viewports,
		viewportState.scissorsset->numScissors,
		viewportState.scissorsset->scissors,
	};
	return createInfo;
}

static VkPipelineRasterizationStateCreateInfo
rasterizationSCI (qfv_pipelinerasterization_t rasterizationState)
{
	VkPipelineRasterizationStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, 0, 0,
		rasterizationState.depthClampEnable,
		rasterizationState.rasterizerDiscardEnable,
		rasterizationState.polygonMode,
		rasterizationState.cullMode,
		rasterizationState.frontFace,
		rasterizationState.depthBiasEnable,
		rasterizationState.depthBiasConstantFactor,
		rasterizationState.depthBiasClamp,
		rasterizationState.depthBiasSlopeFactor,
		rasterizationState.lineWidth,
	};
	return createInfo;
}

static VkPipelineMultisampleStateCreateInfo
multisampleSCI (qfv_pipelinemultisample_t multisampleState)
{
	VkPipelineMultisampleStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, 0, 0,
		multisampleState.rasterizationSamples,
		multisampleState.sampleShadingEnable,
		multisampleState.minSampleShading,
		multisampleState.sampleMask,
		multisampleState.alphaToCoverageEnable,
		multisampleState.alphaToOneEnable,
	};
	return createInfo;
}

static VkPipelineDepthStencilStateCreateInfo
depthStencilSCI (qfv_pipelinedepthandstencil_t depthStencilState)
{
	VkPipelineDepthStencilStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, 0, 0,
		depthStencilState.depthTestEnable,
		depthStencilState.depthWriteEnable,
		depthStencilState.depthCompareOp,
		depthStencilState.depthBoundsTestEnable,
		depthStencilState.stencilTestEnable,
		depthStencilState.front,
		depthStencilState.back,
		depthStencilState.minDepthBounds,
		depthStencilState.maxDepthBounds,
	};
	return createInfo;
}

static VkPipelineColorBlendStateCreateInfo
colorBlendSCI (qfv_pipelineblend_t colorBlendState)
{
	VkPipelineColorBlendStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, 0, 0,
		colorBlendState.logicOpEnable,
		colorBlendState.logicOp,
		colorBlendState.blendAttachments->numAttachments,
		colorBlendState.blendAttachments->attachments,
		{
			colorBlendState.blendConstants[0],
			colorBlendState.blendConstants[1],
			colorBlendState.blendConstants[2],
			colorBlendState.blendConstants[3],
		},
	};
	return createInfo;
}

static VkPipelineDynamicStateCreateInfo
dynamicSCI (qfv_dynamicstateset_t *dynamicState)
{
	VkPipelineDynamicStateCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, 0, 0,
		dynamicState->numStates, dynamicState->states,
	};
	return createInfo;
}

qfv_pipelineset_t *
QFV_CreateGraphicsPipelines (qfv_device_t *device,
							 qfv_pipelinecache_t *cache,
							 qfv_graphicspipelinecreateinfoset_t *gpciSet)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	uint32_t numPipelines = gpciSet->numPipelines;
	uint32_t stageCount = 0;
	for (uint32_t i = 0; i < numPipelines; i++) {
		stageCount += gpciSet->pipelines[i]->stages->size;
	}

	size_t blockSize = numPipelines
		* (sizeof (VkGraphicsPipelineCreateInfo)
		   + sizeof (VkPipelineVertexInputStateCreateInfo)
		   + sizeof (VkPipelineInputAssemblyStateCreateInfo)
		   + sizeof (VkPipelineTessellationStateCreateInfo)
		   + sizeof (VkPipelineViewportStateCreateInfo)
		   + sizeof (VkPipelineRasterizationStateCreateInfo)
		   + sizeof (VkPipelineMultisampleStateCreateInfo)
		   + sizeof (VkPipelineDepthStencilStateCreateInfo)
		   + sizeof (VkPipelineColorBlendStateCreateInfo)
		   + sizeof (VkPipelineDynamicStateCreateInfo))
		+ stageCount * sizeof (VkPipelineShaderStageCreateInfo);
	VkGraphicsPipelineCreateInfo *pipelineInfos = malloc (blockSize);

	pipelineInfos->pStages = (void *)(pipelineInfos + numPipelines);
	pipelineInfos->pVertexInputState
		= (void *)(pipelineInfos->pStages + stageCount);
	pipelineInfos->pInputAssemblyState
		= (void *)(pipelineInfos->pVertexInputState + numPipelines);
	pipelineInfos->pTessellationState
		= (void *)(pipelineInfos->pInputAssemblyState + numPipelines);
	pipelineInfos->pViewportState
		= (void *)(pipelineInfos->pTessellationState + numPipelines);
	pipelineInfos->pRasterizationState
		= (void *)(pipelineInfos->pViewportState + numPipelines);
	pipelineInfos->pMultisampleState
		= (void *)(pipelineInfos->pRasterizationState + numPipelines);
	pipelineInfos->pDepthStencilState
		= (void *)(pipelineInfos->pMultisampleState + numPipelines);
	pipelineInfos->pColorBlendState
		= (void *)(pipelineInfos->pDepthStencilState + numPipelines);
	pipelineInfos->pDynamicState
		= (void *)(pipelineInfos->pColorBlendState + numPipelines);
	for (uint32_t i = 1; i < gpciSet->numPipelines; i++) {
		pipelineInfos[i].pStages = pipelineInfos[i - 1].pStages
			+ gpciSet->pipelines[i - 1]->stages->size;
		pipelineInfos[i].pVertexInputState
			= pipelineInfos[i - 1].pVertexInputState + 1;
		pipelineInfos[i].pInputAssemblyState
			= pipelineInfos[i - 1].pInputAssemblyState + 1;
		pipelineInfos[i].pTessellationState
			= pipelineInfos[i - 1].pTessellationState + 1;
		pipelineInfos[i].pViewportState
			= pipelineInfos[i - 1].pViewportState + 1;
		pipelineInfos[i].pRasterizationState
			= pipelineInfos[i - 1].pRasterizationState + 1;
		pipelineInfos[i].pMultisampleState
			= pipelineInfos[i - 1].pMultisampleState + 1;
		pipelineInfos[i].pDepthStencilState
			= pipelineInfos[i - 1].pDepthStencilState + 1;
		pipelineInfos[i].pColorBlendState
			= pipelineInfos[i - 1].pColorBlendState + 1;
		pipelineInfos[i].pDynamicState
			= pipelineInfos[i - 1].pDynamicState + 1;
	}
	for (uint32_t i = 0; i < gpciSet->numPipelines; i++) {
		VkGraphicsPipelineCreateInfo *gci = pipelineInfos + i;
		qfv_graphicspipelinecreateinfo_t *ci = gpciSet->pipelines[i];
		gci->sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gci->pNext = 0;
		gci->flags = ci->flags;
		gci->stageCount = ci->stages->size;
		for (uint32_t j = 0; j < gci->stageCount; j++) {
			((VkPipelineShaderStageCreateInfo *)gci->pStages)[i] = shaderStageCI (ci->stages->a[j]);
		}
		if (ci->vertexState) {
			*(VkPipelineVertexInputStateCreateInfo *)gci->pVertexInputState = vertexInputSCI (*ci->vertexState);
		} else {
			gci->pVertexInputState = 0;
		}
		if (ci->inputAssemblyState) {
			*(VkPipelineInputAssemblyStateCreateInfo *)gci->pInputAssemblyState = inputAssemblySCI (*ci->inputAssemblyState);
		} else {
			gci->pInputAssemblyState = 0;
		}
		if (ci->tessellationState) {
			*(VkPipelineTessellationStateCreateInfo *)gci->pTessellationState = tessellationSCI (*ci->tessellationState);
		} else {
			gci->pTessellationState = 0;
		}
		if (ci->viewportState) {
			*(VkPipelineViewportStateCreateInfo *)gci->pViewportState = viewportSCI (*ci->viewportState);
		} else {
			gci->pViewportState = 0;
		}
		if (ci->rasterizationState) {
			*(VkPipelineRasterizationStateCreateInfo *)gci->pRasterizationState = rasterizationSCI (*ci->rasterizationState);
		} else {
			gci->pRasterizationState = 0;
		}
		if (ci->multisampleState) {
			*(VkPipelineMultisampleStateCreateInfo *)gci->pMultisampleState = multisampleSCI (*ci->multisampleState);
		} else {
			gci->pMultisampleState = 0;
		}
		if (ci->depthStencilState) {
			*(VkPipelineDepthStencilStateCreateInfo *)gci->pDepthStencilState = depthStencilSCI (*ci->depthStencilState);
		} else {
			gci->pDepthStencilState = 0;
		}
		if (ci->colorBlendState) {
			*(VkPipelineColorBlendStateCreateInfo *)gci->pColorBlendState = colorBlendSCI (*ci->colorBlendState);
		} else {
			gci->pColorBlendState = 0;
		}
		if (ci->dynamicState) {
			*(VkPipelineDynamicStateCreateInfo *)gci->pDynamicState = dynamicSCI (ci->dynamicState);
		} else {
			gci->pDynamicState = 0;
		}
		gci->layout = ci->layout->layout;
		gci->renderPass = ci->renderPass;
		gci->subpass = ci->subpass;
		gci->basePipelineHandle = ci->basePipeline->pipeline;
		gci->basePipelineIndex = ci->basePipelineIndex;
	}

	VkPipeline *pipelines = alloca (numPipelines * sizeof (pipelines));
	dfunc->vkCreateGraphicsPipelines (dev, cache->cache,
									  numPipelines, pipelineInfos, 0,
									  pipelines);

	qfv_pipelineset_t *pipelineset = QFV_AllocPipelineSet (numPipelines, malloc);
	pipelineset->numPipelines = numPipelines;
	for (uint32_t i = 0; i < numPipelines; i++) {
		pipelineset->pipelines[i] = malloc (sizeof (qfv_pipeline_t));
		pipelineset->pipelines[i]->device = device;
		pipelineset->pipelines[i]->pipeline = pipelines[i];
	}
	return pipelineset;
}

qfv_pipelineset_t *
QFV_CreateComputePipelines (qfv_device_t *device,
							qfv_pipelinecache_t *cache,
							qfv_computepipelinecreateinfoset_t *cpciSet)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	uint32_t numPipelines = cpciSet->numPipelines;

	size_t blockSize = numPipelines * (sizeof (VkComputePipelineCreateInfo));
	VkComputePipelineCreateInfo *pipelineInfos = malloc (blockSize);

	for (uint32_t i = 0; i < cpciSet->numPipelines; i++) {
		VkComputePipelineCreateInfo *cci = pipelineInfos + i;
		qfv_computepipelinecreateinfo_t *ci = cpciSet->pipelines[i];
		cci->sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		cci->pNext = 0;
		cci->flags = ci->flags;
		cci->stage = shaderStageCI (*ci->stage);
		cci->layout = ci->layout->layout;
		cci->basePipelineHandle = ci->basePipeline->pipeline;
		cci->basePipelineIndex = ci->basePipelineIndex;
	}

	VkPipeline *pipelines = alloca (numPipelines * sizeof (pipelines));
	dfunc->vkCreateComputePipelines (dev, cache->cache,
									 numPipelines, pipelineInfos, 0,
									 pipelines);

	qfv_pipelineset_t *pipelineset = QFV_AllocPipelineSet (numPipelines, malloc);
	pipelineset->numPipelines = numPipelines;
	for (uint32_t i = 0; i < numPipelines; i++) {
		pipelineset->pipelines[i] = malloc (sizeof (qfv_pipeline_t));
		pipelineset->pipelines[i]->device = device;
		pipelineset->pipelines[i]->pipeline = pipelines[i];
	}
	return pipelineset;
}

void
QFV_DestroyPipeline (qfv_pipeline_t *pipeline)
{
	qfv_device_t *device = pipeline->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyPipeline (dev, pipeline->pipeline, 0);
}
