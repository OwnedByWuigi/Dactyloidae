#include <Mw/Milsko.h>

/**
 * ioixd maintains this file. nishi doesn't know vulkan at all
 */

#ifndef MW_VULKAN
#define MW_VULKAN_NO_INCLUDE
#endif
#include <Mw/Widget/Vulkan.h>

#ifdef MW_VULKAN
#ifdef USE_GDI
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#ifdef USE_X11
#define VK_USE_PLATFORM_XLIB_KHR 1
#endif
#ifdef USE_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#endif
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef USE_GDI
#include <vulkan/vulkan_win32.h>
#endif
#ifdef USE_X11
#include <vulkan/vulkan_xlib.h>
#endif
#ifdef USE_WAYLAND
#include <vulkan/vulkan_wayland.h>
#endif
#ifdef USE_COCOA
#include <vulkan/vulkan_metal.h>
#endif

#ifdef HAS_VK_ENUM_STRING_HELPER
#include <vulkan/vk_enum_string_helper.h>
#endif

#include <stdint.h>
#include <stdbool.h>

#include "../../external/stb_ds.h"

/* convienence macro for handling vulkan errors */
#ifdef HAS_VK_ENUM_STRING_HELPER
#define VK_CMD(func) \
	vk_res = func; \
	if(vk_res != VK_SUCCESS) { \
		char buf[1024]; \
		MwStringPrintIntoBuffer(buf, 1024, "Vulkan error (%s:%d): %s\n", __FILE__, __LINE__, string_VkResult(vk_res)); \
		MwDispatchError(1, buf); \
		return 1; \
	}
#else
#define VK_CMD(func) \
	vk_res = func; \
	if(vk_res != VK_SUCCESS) { \
		char buf[1024]; \
		MwStringPrintIntoBuffer(buf, 1024, "Vulkan error (%s:%d): (error %d)\n", __FILE__, __LINE__, vk_res); \
		MwDispatchError(1, buf); \
		return 1; \
	}
#endif

/* convienence macro for loading a vulkan function pointer into memory */
#define LOAD_VK_FUNCTION(name) \
	PFN_##name _##name = (PFN_##name)o->vkGetInstanceProcAddr(o->vkInstance, #name); \
	VK_ASSERT(_##name);

/* convienence macro to return an error if an assert goes wrong */
#define VK_ASSERT(val) \
	if(!val) { \
		char buf[1024]; \
		MwStringPrintIntoBuffer(buf, 1024, "Vulkan error (%s:%d): Assertion Failed (%s != NULL)\n", __FILE__, __LINE__, #val); \
		MwDispatchError(1, buf); \
		return 1; \
	}

typedef enum vulkan_supported_state_t {
	VULKAN_SUPPORTED_UNKNOWN,
	VULKAN_SUPPORTED_YES,
	VULKAN_SUPPORTED_NO,
} vulkan_supported_state;

vulkan_supported_state vulkan_supported = VULKAN_SUPPORTED_UNKNOWN;

typedef struct vulkan {
	void*			  vulkanLibrary;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
	VkInstance		  vkInstance;
	VkSurfaceKHR		  vkSurface;

	MwVulkanConfig vulkan_config;

	VkPhysicalDevice vkPhysicalDevice;
	VkDevice	 vkLogicalDevice;
	VkQueue		 vkGraphicsQueue;
	VkQueue		 vkPresentQueue;
	uint32_t	 vkGraphicsFamilyIDX;
	uint32_t	 vkPresentFamilyIDX;

	const char** vkInstanceExtensions;
	const char** vkDeviceExtensions;
	const char** vkLayers;
	int	     vkInstanceExtensionCount;
	int	     vkDeviceExtensionCount;
	int	     vkLayerCount;

	const char** enabledExtensions;
	const char** enabledLayers;

} vulkan_t;

static int vulkan_instance_setup(MwWidget handle, vulkan_t* o);
static int vulkan_surface_setup(MwWidget handle, vulkan_t* o);
static int vulkan_devices_setup(MwWidget handle, vulkan_t* o);

static int wcreate(MwWidget handle) {
	int err;
	if(!handle->internal) {
		vulkan_t* o;
		handle->internal = malloc(sizeof(vulkan_t));
		memset(handle->internal, 0, sizeof(vulkan_t));

		o				   = handle->internal;
		o->vulkan_config.api_version	   = VK_API_VERSION_1_0;
		o->vulkan_config.vk_version	   = VK_VERSION_1_0;
		o->vulkan_config.validation_layers = VK_FALSE;
	}

	err = vulkan_instance_setup(handle, handle->internal);
	if(err != 0) {
		return 1;
	}
	err = vulkan_surface_setup(handle, handle->internal);
	if(err != 0) {
		return 1;
	}
	err = vulkan_devices_setup(handle, handle->internal);
	if(err != 0) {
		return 1;
	}

	handle->lowlevel->common.copy_buffer = 0;

	MwSetDefault(handle);

	return 0;
}

static int _destroy(MwWidget handle) {
	vulkan_t* o = (vulkan_t*)handle->internal;

	LOAD_VK_FUNCTION(vkDestroyInstance);
	LOAD_VK_FUNCTION(vkDestroySurfaceKHR);
	LOAD_VK_FUNCTION(vkDestroyDevice);

	free(o->vkLayers);
	free(o->vkDeviceExtensions);
	free(o->vkInstanceExtensions);
	_vkDestroyDevice(o->vkLogicalDevice, NULL);
	_vkDestroySurfaceKHR(o->vkInstance, o->vkSurface, NULL);
	_vkDestroyInstance(o->vkInstance, NULL);
	MwDynamicClose(o->vulkanLibrary);

	free(o);

	return 0;
}

static void destroy(MwWidget handle) {
	int err = _destroy(handle);

	(void)err;
}

static void* vulkan_lib_load() {
#ifdef _WIN32
	return MwDynamicOpen("vulkan-1.dll");
#elif defined(__APPLE__)
	return MwDynamicOpen("libvulkan.dylib");
#else
	return MwDynamicOpen("libvulkan.so");
#endif
}

static int vulkan_instance_setup(MwWidget handle, vulkan_t* o) {
	uint32_t      vulkan_version  = o->vulkan_config.vk_version;
	uint32_t      api_version     = o->vulkan_config.api_version;
	uint32_t      extension_count = 0;
	uint32_t      layer_count     = 0;
	unsigned long i		      = 0;
	unsigned long n		      = 0;

	PFN_vkEnumerateInstanceExtensionProperties
					       _vkEnumerateInstanceExtensionProperties;
	PFN_vkEnumerateInstanceLayerProperties _vkEnumerateInstanceLayerProperties;
	PFN_vkCreateInstance		       _vkCreateInstance;

	VkApplicationInfo    app_info;
	VkInstanceCreateInfo instance_create_info;

	VkExtensionProperties* ext_props;
	VkLayerProperties*     layer_props;

	VkResult vk_res;

	/* Load Vulkan and any other function pointers we need. */
	o->vulkanLibrary = vulkan_lib_load();
	if(!o->vulkanLibrary) {
		vulkan_supported = VULKAN_SUPPORTED_NO;
		MwDispatchError(1, "Vulkan not found.");
	} else {
		vulkan_supported = VULKAN_SUPPORTED_YES;
	}

	o->vkGetInstanceProcAddr = MwDynamicSymbol(o->vulkanLibrary, "vkGetInstanceProcAddr");
	VK_ASSERT(o->vkGetInstanceProcAddr);
	_vkEnumerateInstanceExtensionProperties = MwDynamicSymbol(o->vulkanLibrary, "vkEnumerateInstanceExtensionProperties");
	VK_ASSERT(_vkEnumerateInstanceExtensionProperties);
	_vkEnumerateInstanceLayerProperties = MwDynamicSymbol(o->vulkanLibrary, "vkEnumerateInstanceLayerProperties");
	VK_ASSERT(_vkEnumerateInstanceLayerProperties);
	_vkCreateInstance = MwDynamicSymbol(o->vulkanLibrary, "vkCreateInstance");
	VK_ASSERT(_vkCreateInstance);

	/* setup enabled extensions */
	arrput(o->enabledExtensions, VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		arrput(o->enabledExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		arrput(o->enabledExtensions, VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		arrput(o->enabledExtensions, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
		/* take this opprutunity to set the widget to always render */
		MwWaylandVulkan = MwTRUE;
	}
#endif
#ifdef USE_COCOA
	if(handle->lowlevel->common.type == MwLLBackendCocoa) {
		arrput(o->enabledExtensions, VK_EXT_METAL_SURFACE_EXTENSION_NAME);
	}
#endif

	/* passing null gives us all the extensions provided by the current vulkan implementation */
	VK_CMD(_vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL));
	ext_props = malloc(sizeof(VkExtensionProperties) * extension_count);
	VK_CMD(_vkEnumerateInstanceExtensionProperties(NULL, &extension_count, ext_props));
	o->vkInstanceExtensions = malloc(sizeof(const char*) * (arrlen(o->enabledExtensions) + 1));

	for(i = 0; i < extension_count; i++) {
		for(n = 0; n < (unsigned long)arrlen(o->enabledExtensions); n++) {
			if(strcmp(ext_props[i].extensionName, o->enabledExtensions[n]) == 0) {
				printf("[Vulkan Widget] Enabling extension %s\n", ext_props[i].extensionName);
				o->vkInstanceExtensions[o->vkInstanceExtensionCount] = ext_props[i].extensionName;
				o->vkInstanceExtensionCount++;
				break;
			}
		}
	}

	app_info = (VkApplicationInfo){
	    .sType		= VK_STRUCTURE_TYPE_APPLICATION_INFO,
	    .pNext		= NULL,
	    .pApplicationName	= "",
	    .applicationVersion = vulkan_version,
	    .pEngineName	= "",
	    .engineVersion	= vulkan_version,
	    .apiVersion		= api_version,
	};

	VK_CMD(_vkEnumerateInstanceLayerProperties(&layer_count, NULL));
	layer_props = malloc(sizeof(VkLayerProperties) * layer_count);
	VK_CMD(_vkEnumerateInstanceLayerProperties(&layer_count, layer_props));
	o->vkLayers = malloc(256 * (arrlen(o->enabledLayers) + 1));

	if(o->vulkan_config.validation_layers) {
		arrput(o->enabledLayers, "VK_LAYER_KHRONOS_validation");
	}
	for(i = 0; i < layer_count; i++) {
		for(n = 0; n < (unsigned long)arrlen(o->enabledLayers); n++) {
			if(strcmp(layer_props[i].layerName, o->enabledLayers[n]) == 0) {
				printf("[Vulkan Widget] Enabling layer %s\n", layer_props[i].layerName);
				o->vkLayers[o->vkLayerCount] = layer_props[i].layerName;
				o->vkLayerCount++;
				break;
			}
		}
	}
	o->vkLayers[i++] = NULL;

	instance_create_info = (VkInstanceCreateInfo){
	    .sType		     = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
	    .flags		     = 0,
	    .pApplicationInfo	     = &app_info,
	    .enabledExtensionCount   = o->vkInstanceExtensionCount,
	    .enabledLayerCount	     = o->vkLayerCount,
	    .ppEnabledExtensionNames = o->vkInstanceExtensions,
	    .ppEnabledLayerNames     = o->vkLayers,
	};
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

	VK_CMD(_vkCreateInstance(&instance_create_info, NULL, &o->vkInstance));
	return 0;
}

static int vulkan_surface_setup(MwWidget handle, vulkan_t* o) {
	int vk_res;
#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		LOAD_VK_FUNCTION(vkCreateWin32SurfaceKHR);

		VkWin32SurfaceCreateInfoKHR createInfo = {
		    .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		    .pNext     = NULL,
		    .flags     = 0,
		    .hinstance = handle->lowlevel->gdi.hInstance,
		    .hwnd      = handle->lowlevel->gdi.hWnd,
		};

		VK_CMD(_vkCreateWin32SurfaceKHR(o->vkInstance, &createInfo, NULL,
						&o->vkSurface));
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		LOAD_VK_FUNCTION(vkCreateXlibSurfaceKHR);

		VkXlibSurfaceCreateInfoKHR createInfo = {
		    .sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		    .pNext  = NULL,
		    .flags  = 0,
		    .dpy    = handle->lowlevel->x11.display,
		    .window = handle->lowlevel->x11.window,
		};
		VK_CMD(_vkCreateXlibSurfaceKHR(o->vkInstance, &createInfo, NULL, &o->vkSurface));
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		LOAD_VK_FUNCTION(vkCreateWaylandSurfaceKHR);

		VkWaylandSurfaceCreateInfoKHR createInfo = {
		    .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		    .pNext   = NULL,
		    .flags   = 0,
		    .display = handle->lowlevel->wayland.display,
		    .surface = handle->lowlevel->wayland.framebuffer.surface,
		};
		VK_CMD(_vkCreateWaylandSurfaceKHR(o->vkInstance, &createInfo, NULL, &o->vkSurface));
	}
#endif
#ifdef USE_COCOA
	if(handle->lowlevel->common.type == MwLLBackendCocoa) {
		VkMetalSurfaceCreateInfoEXT MwCocoaGetMetalSurfaceCreateInfo(MwWidget handle);
		VkMetalSurfaceCreateInfoEXT inf = MwCocoaGetMetalSurfaceCreateInfo(handle);

		LOAD_VK_FUNCTION(vkCreateMetalSurfaceEXT);

		VK_CMD(_vkCreateMetalSurfaceEXT(o->vkInstance, &inf, NULL, &o->vkSurface));
	}
#endif
	return 0;
}

static int vulkan_devices_setup(MwWidget handle, vulkan_t* o) {
	(void)(handle);

	int			 vk_res;
	unsigned long		 i, n;
	uint32_t		 deviceCount;
	VkPhysicalDevice*	 devices;
	uint32_t		 queueFamilyCount;
	VkQueueFamilyProperties* family_props;
	float			 queuePriority = 1.0f;
	VkDeviceQueueCreateInfo	 queueCreateInfos[2];
	VkDeviceCreateInfo	 createInfo;
	VkExtensionProperties*	 ext_props;
	uint32_t		 extension_count;
	VkBool32		 has_graphics	  = false;
	VkBool32		 has_present	  = false;
	int			 queueCreateCount = 0;

	LOAD_VK_FUNCTION(vkEnumeratePhysicalDevices);
	LOAD_VK_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
	LOAD_VK_FUNCTION(vkCreateDevice);
	LOAD_VK_FUNCTION(vkGetDeviceQueue);
	LOAD_VK_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
	LOAD_VK_FUNCTION(vkEnumerateDeviceExtensionProperties);

	/* create the physical device */
	VK_CMD(_vkEnumeratePhysicalDevices(o->vkInstance, &deviceCount, NULL));
	devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
	VK_CMD(_vkEnumeratePhysicalDevices(o->vkInstance, &deviceCount, devices));

	for(i = 0; i < deviceCount; i++) {
		_vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, NULL);
		family_props = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
		_vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, family_props);
		o->vkGraphicsFamilyIDX = 0;
		o->vkPresentFamilyIDX  = 0;
		for(n = 0; n < queueFamilyCount; n++) {
			if(family_props[n].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				has_graphics	       = true;
				o->vkGraphicsFamilyIDX = n;
			};

			_vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], n, o->vkSurface,
							      &has_present);
			if(has_present) {
				o->vkPresentFamilyIDX = n;
			}
			if(has_graphics && has_present) {
				o->vkPhysicalDevice = devices[i];
				break;
			}
		}
		free(family_props);
	}
	if(!has_graphics && !has_present) {
		/* rare, yes, but idk maybe some shitty drivers will present this dillema idk. */
		MwDispatchError(1, "There were no devices with either a graphics or presentation queue.\n");
		return 1;
	}

	/* create the logical device */
	queueCreateInfos[queueCreateCount] = (VkDeviceQueueCreateInfo){
	    .sType	      = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
	    .pNext	      = NULL,
	    .flags	      = 0,
	    .queueFamilyIndex = o->vkGraphicsFamilyIDX,
	    .queueCount	      = 1,
	    .pQueuePriorities = &queuePriority,
	};
	if(o->vkGraphicsFamilyIDX == o->vkPresentFamilyIDX) {
		queueCreateInfos[queueCreateCount++] = (VkDeviceQueueCreateInfo){
		    .sType	      = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		    .pNext	      = NULL,
		    .flags	      = 0,
		    .queueFamilyIndex = o->vkPresentFamilyIDX,
		    .queueCount	      = 1,
		    .pQueuePriorities = &queuePriority,
		};
	}

	VK_CMD(_vkEnumerateDeviceExtensionProperties(o->vkPhysicalDevice, NULL, &extension_count, NULL));
	ext_props = malloc(sizeof(VkExtensionProperties) * extension_count);
	VK_CMD(_vkEnumerateDeviceExtensionProperties(o->vkPhysicalDevice, NULL, &extension_count, ext_props));
	o->vkDeviceExtensions = malloc(sizeof(const char*) * (arrlen(o->enabledExtensions) + 1));

	for(i = 0; i < extension_count; i++) {
		for(n = 0; n < (unsigned long)arrlen(o->enabledExtensions); n++) {
			if(strcmp(ext_props[i].extensionName, o->enabledExtensions[n]) == 0) {
				o->vkDeviceExtensions[o->vkDeviceExtensionCount] = ext_props[i].extensionName;
				o->vkDeviceExtensionCount++;
				break;
			}
		}
	}

	createInfo = (VkDeviceCreateInfo){
	    .sType		     = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
	    .pNext		     = NULL,
	    .flags		     = 0,
	    .queueCreateInfoCount    = queueCreateCount,
	    .pQueueCreateInfos	     = queueCreateInfos,
	    .pEnabledFeatures	     = NULL,
	    .enabledExtensionCount   = o->vkDeviceExtensionCount,
	    .ppEnabledExtensionNames = o->vkDeviceExtensions,
	    .enabledLayerCount	     = o->vkLayerCount,
	    .ppEnabledLayerNames     = o->vkLayers,
	};

	VK_CMD(_vkCreateDevice(o->vkPhysicalDevice, &createInfo, NULL, &o->vkLogicalDevice) != VK_SUCCESS);

	_vkGetDeviceQueue(o->vkLogicalDevice, o->vkGraphicsFamilyIDX, 0, &o->vkGraphicsQueue);
	if(o->vkGraphicsFamilyIDX == o->vkPresentFamilyIDX) {
		o->vkPresentQueue = o->vkGraphicsQueue;
	} else {
		_vkGetDeviceQueue(o->vkLogicalDevice, o->vkPresentFamilyIDX, 0, &o->vkPresentQueue);
	}
	free(devices);
	return 0;
}

int MwVulkanSupported(void) {
	if(vulkan_supported == VULKAN_SUPPORTED_UNKNOWN) {
		void* lib = vulkan_lib_load();
		if(lib == NULL) {
			vulkan_supported = VULKAN_SUPPORTED_NO;
		} else {
			vulkan_supported = VULKAN_SUPPORTED_YES;
			MwDynamicClose(lib);
		}
	}
	if(vulkan_supported == VULKAN_SUPPORTED_YES) {
		return 1;
	} else {
		return 0;
	}
};

static void* mwVulkanGetFieldImpl(MwWidget handle, int field, int* err) {
	vulkan_t* o = handle->internal;
	char	  buf[1024];

	switch(field) {
	case MwVULKANFIELD_GETINSTANCEPROCADDR:
		return o->vkGetInstanceProcAddr;
	case MwVULKANFIELD_INSTANCE:
		return o->vkInstance;
	case MwVULKANFIELD_SURFACE:
		return o->vkSurface;
	case MwVULKANFIELD_PHYSICALDEVICE:
		return o->vkPhysicalDevice;
	case MwVULKANFIELD_LOGICALDEVICE:
		return o->vkLogicalDevice;
	case MwVULKANFIELD_GRAPHICSQUEUEINDEX:
		return &o->vkGraphicsFamilyIDX;
	case MwVULKANFIELD_PRESENTQUEUEINDEX:
		return &o->vkPresentFamilyIDX;
	case MwVULKANFIELD_GRAPHICSQUEUE:
		return o->vkGraphicsQueue;
	case MwVULKANFIELD_PRESENTQUEUE:
		return o->vkPresentQueue;
	default:
		MwStringPrintIntoBuffer(buf, 1024, "Unknown vulkan field request (%d given)", field);
		MwDispatchError(1, buf);
		if(err != NULL) {
			*err = 1;
		}
		return NULL;
	}
	if(err != NULL) {
		*err = 0;
	}
}

static void prop_change(MwWidget handle, const char* prop) {
	vulkan_t* o = (vulkan_t*)handle->internal;
	if(!o) {
		handle->internal = malloc(sizeof(vulkan_t));
		o		 = (vulkan_t*)handle->internal;
		memset(o, 0, sizeof(vulkan_t));
		o->vulkan_config.api_version	   = VK_API_VERSION_1_0;
		o->vulkan_config.vk_version	   = VK_VERSION_1_0;
		o->vulkan_config.validation_layers = VK_FALSE;
	}

	if(strcmp(prop, MwNvulkanExtension) == 0) {
		char* str = MwStringDuplicate(MwGetText(handle, MwNvulkanExtension));

		arrput(o->enabledExtensions, str);
	} else if(strcmp(prop, MwNvulkanLayer) == 0) {
		char* str = MwStringDuplicate(MwGetText(handle, MwNvulkanExtension));

		arrput(o->enabledLayers, str);
	} else if(strcmp(prop, MwNvulkanConfig) == 0) {
		memcpy(&o->vulkan_config, MwGetVoid(handle, MwNvulkanConfig), sizeof(MwVulkanConfig));
	}
}

static void func_handler(MwWidget handle, const char* name, void* output, va_list va) {
	if(strcmp(name, "mwVulkanGetField") == 0) {
		int  field	= va_arg(va, int);
		int* err	= va_arg(va, int*);
		*(void**)output = mwVulkanGetFieldImpl(handle, field, err);
	}
}

MwClassRec MwVulkanClassRec = {
    wcreate,	  /* create */
    destroy,	  /* destroy */
    NULL,	  /* draw */
    NULL,	  /* click */
    NULL,	  /* parent_resize */
    prop_change,  /* prop_change */
    NULL,	  /* mouse_move */
    NULL,	  /* mouse_up */
    NULL,	  /* mouse_down */
    NULL,	  /* key */
    func_handler, /* execute */
    NULL,	  /* tick */
    NULL,	  /* resize */
    NULL,	  /* children_update */
    NULL,	  /* children_prop_change */
    NULL,	  /* clipboard */
    NULL,	  /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwVulkanClass = &MwVulkanClassRec;
#else
MwClass MwVulkanClass = NULL;

void MwVulkanConfigure(MwVulkanConfig* cfg) {
	(void)cfg;
}

int MwVulkanSupported(void) {
	return 0;
}
#endif
