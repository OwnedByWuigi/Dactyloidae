#include <Mw/Milsko.h>
#include <Mw/Widget/Vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#ifdef HAS_VK_ENUM_STRING_HELPER
#include <vulkan/vk_enum_string_helper.h>
#else
char buffer[512];

char* string_VkResult(VkResult res) {
	MwStringPrintIntoBuffer(buffer, 512, "%d", res);

	return &buffer[0];
}
#endif

MwWidget window, vulkan;
int	 ow = 300;
int	 oh = 250;

double timer = 0;

PFN_vkGetInstanceProcAddr _vkGetInstanceProcAddr;
VkInstance		  instance;
VkDevice		  device;
VkPhysicalDevice	  physicalDevice;
VkSurfaceKHR		  surface;
VkQueue			  graphicsQueue;
VkQueue			  presentQueue;
uint32_t*		  graphicsQueueIndex;
uint32_t*		  presentQueueIndex;

VkSwapchainKHR swapchain;
VkImage*       swapchainImages;
VkImageView*   swapchainImageView;
uint32_t       swapchainImageViewCount;
uint32_t       currentImageIndex;
uint32_t       frameNumber;

VkFence*     fences;
VkSemaphore* renderFinishedSemaphores;
VkSemaphore* imageAvaliableSemaphores;

VkFramebuffer* framebuffers;

VkPipeline	 pipeline;
VkCommandBuffer* cmdBuffers;
VkRenderPass	 renderPass;

VkResult res;

VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

#define MAX_FRAMES_IN_FLIGHT swapchainImageViewCount

// convienence macro for loading a vulkan function pointer into memory
#define LOAD_VK_FUNCTION(name) \
	PFN_##name _##name = (PFN_##name)_vkGetInstanceProcAddr(instance, #name); \
	assert(_##name);

void MWAPI tick(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	VkCommandBufferBeginInfo beginInfo	= {};
	VkRenderPassBeginInfo	 renderPassInfo = {};
	VkSubmitInfo		 submitInfo	= {};
	VkPresentInfoKHR	 presentInfo	= {};

	VkClearValue clearColor	   = {{{sin(timer) / 10., cos(timer) / 10., atan(timer) / 10., 1.0f}}};
	uint32_t     vertexCount   = 3;
	uint32_t     instanceCount = 1;
	timer += (float)(rand() % 1000) / 1000.;

	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT};

	LOAD_VK_FUNCTION(vkCreateFence);
	LOAD_VK_FUNCTION(vkWaitForFences);
	LOAD_VK_FUNCTION(vkResetFences);
	LOAD_VK_FUNCTION(vkResetCommandBuffer);
	LOAD_VK_FUNCTION(vkBeginCommandBuffer);
	LOAD_VK_FUNCTION(vkEndCommandBuffer);
	LOAD_VK_FUNCTION(vkQueueSubmit);
	LOAD_VK_FUNCTION(vkAcquireNextImageKHR);
	LOAD_VK_FUNCTION(vkQueuePresentKHR);
	LOAD_VK_FUNCTION(vkCreateSwapchainKHR);

	LOAD_VK_FUNCTION(vkCmdBeginRenderPass);
	LOAD_VK_FUNCTION(vkCmdEndRenderPass);
	LOAD_VK_FUNCTION(vkCmdDraw);
	LOAD_VK_FUNCTION(vkCmdBindPipeline);

	if((res = _vkWaitForFences(device, 1, &fences[frameNumber], VK_TRUE, UINT64_MAX)) != VK_SUCCESS) {
		printf("error waiting on fence: %s\n", string_VkResult(res));
		exit(0);
	}

	if((res = _vkResetFences(device, 1, &fences[frameNumber])) != VK_SUCCESS) {
		printf("error resetting fence: %s\n", string_VkResult(res));
		exit(0);
	}

swapchainRetry:
	if((res = _vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvaliableSemaphores[frameNumber], NULL, &currentImageIndex)) != VK_SUCCESS) {
		if(res == VK_ERROR_OUT_OF_DATE_KHR) {
			if(_vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain) != VK_SUCCESS) {
				printf("failed to create swapchain: %s\n", string_VkResult(res));
				exit(0);
			};
			goto swapchainRetry;
		}
	};

	beginInfo.sType		   = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext		   = NULL;
	beginInfo.flags		   = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = NULL;

	if((res = _vkResetCommandBuffer(cmdBuffers[frameNumber], 0)) != VK_SUCCESS) {
		printf("error beginning command buffer record: %s\n", string_VkResult(res));
		exit(0);
	}

	if((res = _vkBeginCommandBuffer(cmdBuffers[frameNumber], &beginInfo)) != VK_SUCCESS) {
		printf("error beginning command buffer record: %s\n", string_VkResult(res));
		exit(0);
	}

	renderPassInfo.sType		 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.pNext		 = NULL;
	renderPassInfo.renderPass	 = renderPass;
	renderPassInfo.framebuffer	 = framebuffers[frameNumber];
	renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
	renderPassInfo.renderArea.extent = (VkExtent2D){(uint32_t)ow, (uint32_t)oh};
	renderPassInfo.clearValueCount	 = 1;
	renderPassInfo.pClearValues	 = &clearColor;

	_vkCmdBeginRenderPass(cmdBuffers[frameNumber], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	_vkCmdBindPipeline(cmdBuffers[frameNumber], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	_vkCmdDraw(cmdBuffers[frameNumber], vertexCount, instanceCount, 0, 0);

	_vkCmdEndRenderPass(cmdBuffers[frameNumber]);

	if((res = _vkEndCommandBuffer(cmdBuffers[frameNumber])) != VK_SUCCESS) {
		printf("error recording command buffer: %s\n", string_VkResult(res));
		exit(0);
	}

	submitInfo.sType		= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext		= NULL;
	submitInfo.waitSemaphoreCount	= 1;
	submitInfo.pWaitSemaphores	= &imageAvaliableSemaphores[frameNumber];
	submitInfo.pWaitDstStageMask	= waitStages;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers	= &cmdBuffers[frameNumber];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores	= &renderFinishedSemaphores[frameNumber];

	if((res = _vkQueueSubmit(graphicsQueue, 1, &submitInfo, fences[frameNumber])) != VK_SUCCESS) {
		printf("error submitting command buffer: %s\n", string_VkResult(res));
		exit(0);
	}

	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = &renderFinishedSemaphores[frameNumber];
	presentInfo.swapchainCount     = 1;
	presentInfo.pSwapchains	       = &swapchain;
	presentInfo.pImageIndices      = &currentImageIndex;

	presentInfo.pResults = NULL; // Optional

	_vkQueuePresentKHR(presentQueue, &presentInfo);

	// frameNumber = currentImageIndex;
	frameNumber = (frameNumber + 1) % (MAX_FRAMES_IN_FLIGHT);
}

void vulkan_setup(MwWidget handle) {
	FILE*  vertFile;
	FILE*  fragFile;
	void*  vertBuf;
	void*  fragBuf;
	size_t vertFileSize;
	size_t fragFileSize;
	size_t amountRead;

	uint32_t		i;
	VkViewport		viewport = {};
	VkRect2D		scissor	 = {};
	VkCommandPool		cmdPool	 = VK_NULL_HANDLE;
	VkResult		res	 = 0;
	VkShaderModule		fragShaderModule;
	VkShaderModule		vertShaderModule;
	VkPipelineLayout	pipelineLayout	   = VK_NULL_HANDLE;
	VkAttachmentDescription colorAttachment	   = {};
	VkAttachmentReference	colorAttachmentRef = {};
	VkSubpassDescription	subpass		   = {};

	VkRenderPassCreateInfo		       renderPassInfo	    = {};
	VkShaderModuleCreateInfo	       vertInfo		    = {};
	VkShaderModuleCreateInfo	       fragInfo		    = {};
	VkPipelineLayoutCreateInfo	       pipelineLayoutInfo   = {};
	VkPipelineShaderStageCreateInfo	       vertShaderStageInfo  = {};
	VkPipelineShaderStageCreateInfo	       fragShaderStageInfo  = {};
	VkPipelineShaderStageCreateInfo	       shaderStages[2]	    = {};
	VkPipelineVertexInputStateCreateInfo   vertexInputInfo	    = {};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly	    = {};
	VkPipelineViewportStateCreateInfo      viewportState	    = {};
	VkPipelineRasterizationStateCreateInfo rasterizer	    = {};
	VkPipelineMultisampleStateCreateInfo   multisampling	    = {};
	VkPipelineColorBlendAttachmentState    colorBlendAttachment = {};
	VkPipelineColorBlendStateCreateInfo    colorBlending	    = {};
	VkGraphicsPipelineCreateInfo	       pipelineInfo	    = {};
	VkFramebufferCreateInfo		       framebufferInfo	    = {};
	VkCommandBufferAllocateInfo	       allocInfo	    = {};
	VkCommandPoolCreateInfo		       poolInfo		    = {};
	VkImageViewCreateInfo		       imageViewCreateInfo  = {};
	VkSemaphoreCreateInfo		       semaphoreInfo	    = {};
	VkFenceCreateInfo		       fenceInfo	    = {};

	int err		       = 0;
	_vkGetInstanceProcAddr = MwVulkanGetField(handle, MwVULKANFIELD_GETINSTANCEPROCADDR, &err);
	instance	       = MwVulkanGetField(handle, MwVULKANFIELD_INSTANCE, &err);
	device		       = MwVulkanGetField(handle, MwVULKANFIELD_LOGICALDEVICE, &err);
	physicalDevice	       = MwVulkanGetField(handle, MwVULKANFIELD_PHYSICALDEVICE, &err);
	graphicsQueue	       = MwVulkanGetField(handle, MwVULKANFIELD_GRAPHICSQUEUE, &err);
	presentQueue	       = MwVulkanGetField(handle, MwVULKANFIELD_PRESENTQUEUE, &err);
	surface		       = MwVulkanGetField(handle, MwVULKANFIELD_SURFACE, &err);
	presentQueueIndex      = MwVulkanGetField(handle, MwVULKANFIELD_PRESENTQUEUEINDEX, &err);
	graphicsQueueIndex     = MwVulkanGetField(handle, MwVULKANFIELD_GRAPHICSQUEUEINDEX, &err);

	LOAD_VK_FUNCTION(vkCreateShaderModule);
	LOAD_VK_FUNCTION(vkCreatePipelineLayout);
	LOAD_VK_FUNCTION(vkCreateGraphicsPipelines);
	LOAD_VK_FUNCTION(vkCreateCommandPool);
	LOAD_VK_FUNCTION(vkCreateFramebuffer);
	LOAD_VK_FUNCTION(vkAllocateCommandBuffers);
	LOAD_VK_FUNCTION(vkCreateFence);
	LOAD_VK_FUNCTION(vkCreateSemaphore);
	LOAD_VK_FUNCTION(vkCreateSwapchainKHR);
	LOAD_VK_FUNCTION(vkGetSwapchainImagesKHR);
	LOAD_VK_FUNCTION(vkCreateImageView);
	LOAD_VK_FUNCTION(vkCreateRenderPass);

	// create a swapchain
	swapchainCreateInfo.sType	     = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface	     = surface;
	swapchainCreateInfo.minImageCount    = 3;
	swapchainCreateInfo.imageFormat	     = VK_FORMAT_B8G8R8A8_SRGB;
	swapchainCreateInfo.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainCreateInfo.imageExtent	     = (VkExtent2D){.width = ow, .height = oh},
	swapchainCreateInfo.imageArrayLayers = 1,
	swapchainCreateInfo.imageUsage	     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
					 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					 VK_IMAGE_USAGE_SAMPLED_BIT;
	// th is how we specify no transformation.
	swapchainCreateInfo.preTransform   = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode	   = VK_PRESENT_MODE_IMMEDIATE_KHR;
	// we don't care about the color of pixels that are obscured.
	swapchainCreateInfo.clipped	 = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = NULL;

	if(*graphicsQueueIndex != *presentQueueIndex) {
		swapchainCreateInfo.imageSharingMode	  = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;

		uint32_t indices[] = {
		    *graphicsQueueIndex,
		    *presentQueueIndex,
		};
		swapchainCreateInfo.pQueueFamilyIndices = indices;
	} else {
		swapchainCreateInfo.imageSharingMode	  = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices	  = NULL;
	}

	if(_vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain) != VK_SUCCESS) {
		printf("failed to create swapchain: %s\n", string_VkResult(res));
		exit(0);
	};

	imageViewCreateInfo.sType			    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.viewType			    = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format			    = swapchainCreateInfo.imageFormat;
	imageViewCreateInfo.components.r		    = VK_COMPONENT_SWIZZLE_R;
	imageViewCreateInfo.components.g		    = VK_COMPONENT_SWIZZLE_G;
	imageViewCreateInfo.components.b		    = VK_COMPONENT_SWIZZLE_B;
	imageViewCreateInfo.components.a		    = VK_COMPONENT_SWIZZLE_A;
	imageViewCreateInfo.subresourceRange.aspectMask	    = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
	imageViewCreateInfo.subresourceRange.levelCount	    = VK_REMAINING_MIP_LEVELS;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount	    = VK_REMAINING_ARRAY_LAYERS;

	if(_vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageViewCount, NULL) != VK_SUCCESS) {
		printf("failed to get swapchain images: %s\n", string_VkResult(res));
		exit(0);
	}
	swapchainImages	   = malloc(sizeof(VkImage) * MAX_FRAMES_IN_FLIGHT);
	swapchainImageView = malloc(sizeof(VkImageView) * MAX_FRAMES_IN_FLIGHT);
	_vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageViewCount, swapchainImages);
	for(i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		imageViewCreateInfo.image = swapchainImages[i];
		if(_vkCreateImageView(device, &imageViewCreateInfo, NULL, &swapchainImageView[i]) != VK_SUCCESS) {
			printf("failed to get swapchain images: %s\n", string_VkResult(res));
			exit(0);
		}
	}

	//  Create a Render Pass.
	colorAttachment.flags	       = 0;
	colorAttachment.format	       = swapchainCreateInfo.imageFormat;
	colorAttachment.samples	       = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp	       = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp	       = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttachmentRef.attachment  = 0;
	colorAttachmentRef.layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpass.flags			= 0;
	subpass.pipelineBindPoint	= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount	= 0;
	subpass.pInputAttachments	= NULL;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments	= &colorAttachmentRef;
	subpass.pResolveAttachments	= NULL;
	subpass.pDepthStencilAttachment = NULL;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments	= NULL;

	renderPassInfo.sType	       = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext	       = NULL;
	renderPassInfo.flags	       = 0;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments    = &colorAttachment;
	renderPassInfo.subpassCount    = 1;
	renderPassInfo.pSubpasses      = &subpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies   = NULL;

	if((res = _vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass)) != VK_SUCCESS) {
		printf("error creating the render pass: %s\n", string_VkResult(res));
		exit(0);
	}

	// Create the Vertex Shader Module.
	vertFile = fopen("triangle.vert.spv", "rb");
	if(vertFile == NULL) vertFile = fopen("examples/vkdemos/triangle.vert.spv", "rb");
	fragFile = fopen("triangle.frag.spv", "rb");
	if(fragFile == NULL) fragFile = fopen("examples/vkdemos/triangle.frag.spv", "rb");

	fseek(vertFile, 0L, SEEK_END);
	vertFileSize = ftell(vertFile);
	rewind(vertFile);
	vertBuf = malloc(vertFileSize);
	memset(vertBuf, 0, vertFileSize);
	amountRead = fread(vertBuf, 1, vertFileSize, vertFile);
	printf("triangle.vert.spv: read %zu bytes\n", amountRead);

	vertInfo.sType	  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertInfo.pNext	  = NULL;
	vertInfo.flags	  = 0;
	vertInfo.codeSize = amountRead;
	vertInfo.pCode	  = vertBuf;

	if(_vkCreateShaderModule(device, &vertInfo, NULL, &vertShaderModule) != VK_SUCCESS) {
		printf("failed to create the shader module: %s\n", string_VkResult(res));
		exit(0);
	}

	// Create the Fragment Shader Module.
	fseek(fragFile, 0L, SEEK_END);
	fragFileSize = ftell(fragFile);
	rewind(fragFile);
	fragBuf = malloc(fragFileSize);
	memset(fragBuf, 0, fragFileSize);
	amountRead = fread(fragBuf, 1, fragFileSize, fragFile);
	printf("triangle.frag.spv: read %zu bytes\n", amountRead);

	fragInfo.sType	  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragInfo.pNext	  = NULL;
	fragInfo.flags	  = 0;
	fragInfo.codeSize = amountRead;
	fragInfo.pCode	  = fragBuf;

	if(_vkCreateShaderModule(device, &fragInfo, NULL, &fragShaderModule) != VK_SUCCESS) {
		printf("error creating the shader module: %s\n", string_VkResult(res));
		exit(0);
	}

	// Create Pipeline Layout.
	pipelineLayoutInfo.sType		  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext		  = NULL;
	pipelineLayoutInfo.flags		  = 0;
	pipelineLayoutInfo.setLayoutCount	  = 0;
	pipelineLayoutInfo.pSetLayouts		  = NULL;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if((res = _vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout)) != VK_SUCCESS) {
		printf("error creating the pipeline layout: %s\n", string_VkResult(res));
		exit(0);
	}

	vertShaderStageInfo.sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.pNext		= NULL;
	vertShaderStageInfo.flags		= 0;
	vertShaderStageInfo.stage		= VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module		= vertShaderModule;
	vertShaderStageInfo.pName		= "main";
	vertShaderStageInfo.pSpecializationInfo = NULL;

	fragShaderStageInfo.sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.pNext		= NULL;
	fragShaderStageInfo.flags		= 0;
	fragShaderStageInfo.stage		= VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module		= fragShaderModule;
	fragShaderStageInfo.pName		= "main";
	fragShaderStageInfo.pSpecializationInfo = NULL;

	shaderStages[0] = vertShaderStageInfo;
	shaderStages[1] = fragShaderStageInfo;

	vertexInputInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext				= NULL;
	vertexInputInfo.flags				= 0;
	vertexInputInfo.vertexBindingDescriptionCount	= 0;
	vertexInputInfo.pVertexBindingDescriptions	= NULL;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions	= NULL;

	inputAssembly.sType		     = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.pNext		     = NULL;
	inputAssembly.flags		     = 0;
	inputAssembly.topology		     = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	viewport.x	  = 0.0f;
	viewport.y	  = 0.0f;
	viewport.width	  = (float)ow;
	viewport.height	  = (float)oh;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset = (VkOffset2D){0, 0};
	scissor.extent = (VkExtent2D){(uint32_t)viewport.width, (uint32_t)viewport.height};

	viewportState		    = (VkPipelineViewportStateCreateInfo){};
	viewportState.sType	    = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports    = &viewport;
	viewportState.scissorCount  = 1;
	viewportState.pScissors	    = &scissor;

	rasterizer.sType		   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.pNext		   = NULL;
	rasterizer.flags		   = 0;
	rasterizer.depthClampEnable	   = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode		   = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode		   = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace		   = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable	   = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0;
	rasterizer.depthBiasClamp	   = 0.0;
	rasterizer.depthBiasSlopeFactor	   = 0.0;
	rasterizer.lineWidth		   = 1.0f;

	multisampling.sType		    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.pNext		    = NULL;
	multisampling.flags		    = 0;
	multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable   = VK_FALSE;
	multisampling.minSampleShading	    = 0.0;
	multisampling.pSampleMask	    = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable	    = VK_FALSE;

	colorBlendAttachment.blendEnable	 = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp	 = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp	 = VK_BLEND_OP_ADD;
	colorBlendAttachment.colorWriteMask	 = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	colorBlending.sType		= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext		= NULL;
	colorBlending.flags		= 0;
	colorBlending.logicOpEnable	= VK_FALSE;
	colorBlending.logicOp		= VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount	= 1;
	colorBlending.pAttachments	= &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	pipelineInfo.sType		 = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext		 = NULL;
	pipelineInfo.flags		 = 0;
	pipelineInfo.stageCount		 = 2;
	pipelineInfo.pStages		 = shaderStages;
	pipelineInfo.pVertexInputState	 = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pTessellationState	 = NULL;
	pipelineInfo.pViewportState	 = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState	 = &multisampling;
	pipelineInfo.pDepthStencilState	 = NULL;
	pipelineInfo.pColorBlendState	 = &colorBlending;
	pipelineInfo.pDynamicState	 = NULL;
	pipelineInfo.layout		 = pipelineLayout;
	pipelineInfo.renderPass		 = renderPass;
	pipelineInfo.subpass		 = 0;
	pipelineInfo.basePipelineHandle	 = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex	 = 0;

	if((res = _vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline)) != VK_SUCCESS) {
		printf("failed to create graphics pipeline: %s\n", string_VkResult(res));
		exit(0);
	}

	framebuffers = malloc(sizeof(VkFramebuffer) * MAX_FRAMES_IN_FLIGHT);

	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.pNext		= NULL;
	framebufferInfo.flags		= 0;
	framebufferInfo.renderPass	= renderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.width		= ow;
	framebufferInfo.height		= oh;
	framebufferInfo.layers		= 1;
	for(i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		framebufferInfo.pAttachments = &swapchainImageView[i];
		if((res = _vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i])) != VK_SUCCESS) {
			printf("error creating the frame buffer: %s\n", string_VkResult(res));
			exit(0);
		}
	}

	// Create Command Pool.
	poolInfo.sType		  = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext		  = NULL;
	poolInfo.flags		  = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = *graphicsQueueIndex;

	if((res = _vkCreateCommandPool(device, &poolInfo, NULL, &cmdPool)) != VK_SUCCESS) {
		printf("error creating the command pool: %s\n", string_VkResult(res));
		exit(0);
	}

	// Create Command Buffer to record draw commands.
	allocInfo.sType		     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.pNext		     = NULL;
	allocInfo.commandPool	     = cmdPool;
	allocInfo.level		     = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	cmdBuffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

	for(i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if((res = _vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffers[i])) != VK_SUCCESS) {
			printf("error allocating the command buffers: %s\n", string_VkResult(res));
			exit(0);
		}
	}

	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = 0;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = 0;
	semaphoreInfo.flags = VK_SEMAPHORE_TYPE_BINARY;

	imageAvaliableSemaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	fences			 = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);

	for(i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if(_vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvaliableSemaphores[i]) != VK_SUCCESS) {
			printf("error creating fence: %s\n", string_VkResult(res));
			exit(0);
		}
		if(_vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			printf("error creating fence: %s\n", string_VkResult(res));
			exit(0);
		}
		if(_vkCreateFence(device, &fenceInfo, NULL, &fences[i]) != VK_SUCCESS) {
			printf("error creating fence: %s\n", string_VkResult(res));
			exit(0);
		}
	}
}

int main() {
	MwLibraryInit();

	if(!MwVulkanSupported()) {
		printf("Vulkan not found or unsupported on target platform.\n");
		return 1;
	}

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 400, 450,
				  MwNtitle, "hello world",
				  NULL);

	vulkan = MwVaCreateWidget(MwVulkanClass, "vulkan", window, 50, 50, ow, oh,
				  MwNvulkanExtension, VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				  NULL);

	MwAddUserHandler(window, MwNtickHandler, tick, NULL);

	vulkan_setup(vulkan);

	MwLoop(window);
}
