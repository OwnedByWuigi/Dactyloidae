/*!
 * @file Mw/Widget/Vulkan.h
 * @brief Vulkan widget
 * @warning This header is not documented yet
 */

/**
 * ioixd maintains this file. nishi doesn't know vulkan at all
 */

#ifndef __MW_WIDGET_VULKAN_H__
#define __MW_WIDGET_VULKAN_H__

#if defined(MW_VULKAN) && !defined(_WIN32) && !defined(__linux__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
#error Vulkan is unsupported on the requested platform.
#endif

#if !defined(MW_VULKAN_NO_INCLUDE)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#endif

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Vulkan widget class
 */
MWDECL MwClass MwVulkanClass;

/*!
 * @brief Configuration options that can be passed to setup parts of the Vulkan widget.
 */
typedef struct _MwVulkanConfig {
	/*!
	 * @brief Vulkan API version (default: VK_API_VERSION_1_0)
	 */
	MwU32 api_version;
	/*!
	 * @brief Vulkan version (default: VK_VERSION_1_0)
	 */
	MwU32 vk_version;
	/*!
	 * @brief Whether or not to enable validation layers (default: false)
	 */
	int validation_layers;
} MwVulkanConfig;

/*!
 * @brief Field that can be gotten from Vulkan.
 */
enum MwVULKANFIELD {
	/*!
	 * @brief The address of the vulkan widget's vkGetInstanceProcAddr function (PFN_vkGetInstanceProcAddr)
	 */
	MwVULKANFIELD_GETINSTANCEPROCADDR = 0,
	/*!
	 * @brief The address of the vulkan widget's instance (VkInstance)
	 */
	MwVULKANFIELD_INSTANCE,
	/*!
	 * @brief The address of the vulkan widget's surface (VkSurfaceKHR)
	 */
	MwVULKANFIELD_SURFACE,
	/*!
	 * @brief The address of the vulkan widget's physical device (VkPhysicalDevice)
	 */
	MwVULKANFIELD_PHYSICALDEVICE,
	/*!
	 * @brief The address of the vulkan widget's logical device (VkDevice)
	 */
	MwVULKANFIELD_LOGICALDEVICE,
	/*!
	 * @brief The address of the index that the vulkan widget uses for the graphics queue (uint32_t *)
	 */
	MwVULKANFIELD_GRAPHICSQUEUEINDEX,
	/*!
	 * @brief The address of the index that the vulkan widget uses for the present queue (uint32_t *)
	 */
	MwVULKANFIELD_PRESENTQUEUEINDEX,
	MwVULKANFIELD_GRAPHICSQUEUE,
	/*!
	 * @brief The address of the vulkan widget's graphics queue (VkQueue)
	 */
	MwVULKANFIELD_PRESENTQUEUE,
};

/*!
 * @brief Function for getting a field from within Vulkan.
 * @note Consult the documentation for MwVULKANFIELD to know what type is expected for output.
 */
MwInline void* MwVulkanGetField(MwWidget handle, int field, int* err) {
	void* field_out;
	MwVaWidgetExecute(handle, "mwVulkanGetField", &field_out, field, err);
	return field_out;
}

/*!
 * @brief Return whether Vulkan is installed on the target platform.
 */
MWDECL int MWAPI MwVulkanSupported(void);

#ifdef __cplusplus
}
#endif

#endif
