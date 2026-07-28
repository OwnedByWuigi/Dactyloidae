#ifndef __APPLE__

#include <Mw/Milsko.h>

#ifndef MW_OPENGL
#define MW_OPENGL_NO_INCLUDE
#endif
#include <Mw/Widget/OpenGL.h>

#ifdef MW_OPENGL
#ifdef USE_GDI
typedef HGLRC(WINAPI* MWwglCreateContext)(HDC);
typedef BOOL(WINAPI* MWwglMakeCurrent)(HDC, HGLRC);
typedef PROC(WINAPI* MWwglGetProcAddress)(LPCSTR);
typedef BOOL(WINAPI* MWwglDeleteContext)(HGLRC);

typedef struct gdiopengl {
	HDC   dc;
	HGLRC gl;

	void* lib;

	MWwglCreateContext  wglCreateContext;
	MWwglMakeCurrent    wglMakeCurrent;
	MWwglDeleteContext  wglDeleteContext;
	MWwglGetProcAddress wglGetProcAddress;
} gdiopengl_t;
#endif
#ifdef USE_X11
typedef XVisualInfo* (*MWglXChooseVisual)(Display* dpy, int screen,
					  int* attribList);
typedef GLXContext (*MWglXCreateContext)(Display* dpy, XVisualInfo* vis,
					 GLXContext shareList, Bool direct);
typedef void (*MWglXDestroyContext)(Display* dpy, GLXContext ctx);
typedef Bool (*MWglXMakeCurrent)(Display* dpy, GLXDrawable drawable,
				 GLXContext ctx);
typedef void (*MWglXSwapBuffers)(Display* dpy, GLXDrawable drawable);
typedef void* (*MWglXGetProcAddress)(const GLubyte* procname);

typedef struct x11opengl {
	XVisualInfo* visual;
	GLXContext   gl;

	void* lib;

	MWglXChooseVisual   glXChooseVisual;
	MWglXCreateContext  glXCreateContext;
	MWglXDestroyContext glXDestroyContext;
	MWglXMakeCurrent    glXMakeCurrent;
	MWglXSwapBuffers    glXSwapBuffers;
	MWglXGetProcAddress glXGetProcAddress;
} x11opengl_t;
#endif

#ifdef USE_WAYLAND
#include <EGL/egl.h>
#include <wayland-egl-core.h>
#include <sys/mman.h>
#include <gbm.h>

/*
Note that for Wayland we create a GBM buffer manually and handle blitting it ourselves instead of using wayland-egl. Doing this allows us greater freedom over event handling; wayland-egl constantly pumps events, making us have to add a weird exception to wayland.c to make this work. Doing this also allows us to clip the OpenGL widget, something that was harder (impossible) with egl wayland.
*/
typedef struct waylandopengl {
	EGLDisplay	    egl_display;
	EGLContext	    egl_context;
	EGLSurface	    egl_surface;
	EGLConfig	    egl_config;
	struct gbm_device*  gbm;
	struct gbm_surface* gbm_surface;
	struct gbm_bo*	    previous_bo;
	int		    drm_fd;
	MwLLPixmap	    frontbuffer;
	MwU8*		    frontbuffer_data;

	void* gllib;
	void* gbmlib;
	EGLBoolean (*eglGetConfigs)(EGLDisplay display,
				    EGLConfig* configs,
				    EGLint     config_size,
				    EGLint*    num_config);
	EGLBoolean (*eglInitialize)(EGLDisplay display,
				    EGLint*    major,
				    EGLint*    minor);
	EGLContext (*eglCreateContext)(EGLDisplay    display,
				       EGLConfig     config,
				       EGLContext    share_context,
				       EGLint const* attrib_list);
	EGLBoolean (*eglMakeCurrent)(EGLDisplay display,
				     EGLSurface draw,
				     EGLSurface read,
				     EGLContext context);
	void* (*eglGetProcAddress)(char const* procname);
	EGLBoolean (*eglSwapBuffers)(EGLDisplay display,
				     EGLSurface surface);
	EGLBoolean (*eglSwapInterval)(EGLDisplay display,
				      EGLint	 interval);
	EGLint (*eglGetError)(void);
	EGLBoolean (*eglBindAPI)(EGLenum api);
	EGLDisplay (*eglGetDisplay)(NativeDisplayType native_display);
	EGLDisplay (*eglGetPlatformDisplay)(EGLenum platform, void* native_display, const EGLAttrib* attrib_list);
	EGLSurface (*eglCreatePlatformWindowSurface)(EGLDisplay	   display,
						     EGLConfig	   config,
						     void*	   native_window,
						     EGLint const* attrib_list);
	EGLBoolean (*eglChooseConfig)(EGLDisplay    display,
				      EGLint const* attrib_list,
				      EGLConfig*    configs,
				      EGLint	    config_size,
				      EGLint*	    num_config);
	EGLBoolean (*eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value);
	EGLBoolean (*eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);

	struct gbm_device* (*gbm_create_device)(int fd);
	struct gbm_surface* (*gbm_surface_create)(struct gbm_device* gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
	void (*gbm_surface_destroy)(struct gbm_surface* surface);

	struct gbm_bo* (*gbm_surface_lock_front_buffer)(struct gbm_surface* surface);
	union gbm_bo_handle (*gbm_bo_get_handle)(struct gbm_bo* bo);
	uint32_t (*gbm_bo_get_stride)(struct gbm_bo* bo);
	uint32_t (*gbm_bo_get_width)(struct gbm_bo* bo);
	uint32_t (*gbm_bo_get_height)(struct gbm_bo* bo);
	void (*gbm_surface_release_buffer)(struct gbm_surface* surface, struct gbm_bo* bo);
	void* (*gbm_bo_map)(struct gbm_bo* bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t flags, uint32_t* stride, void** map_data);
	void (*gbm_bo_unmap)(struct gbm_bo* bo, void* map_data);

} waylandopengl_t;
#endif

static int wcreate(MwWidget handle) {
	void*	 r = NULL;
	MwWidget w = handle;

#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		PIXELFORMATDESCRIPTOR pfd;
		int		      pf;
		gdiopengl_t*	      o = r = malloc(sizeof(*o));

		memset(&pfd, 0, sizeof(pfd));
		pfd.nSize      = sizeof(pfd);
		pfd.nVersion   = 1;
		pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.cDepthBits = 32;
		pfd.cColorBits = 32;

		o->dc = GetDC(handle->lowlevel->gdi.hWnd);

		pf = ChoosePixelFormat(o->dc, &pfd);
		SetPixelFormat(o->dc, pf, &pfd);

		o->lib = MwDynamicOpen("opengl32.dll");
		if(!o->lib) {
			printf("Could not load OpenGL widget under WGL!\n");
			return 1;
		}

#define WGL_FUNC(x) \
	o->x = MwDynamicSymbol(o->lib, #x); \
	if(o->x == NULL) { \
		printf("Could not load OpenGL widget under WGL! " #x " missing.\n"); \
		return 1; \
	}

		WGL_FUNC(wglCreateContext);
		WGL_FUNC(wglMakeCurrent);
		WGL_FUNC(wglDeleteContext);
		WGL_FUNC(wglGetProcAddress);

		o->gl = o->wglCreateContext(o->dc);
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		int	     attribs[5];
		const char*  glpath[] = {"libGL.so", "/usr/local/lib/libGL.so",
					 "/usr/X11R7/lib/libGL.so", "/usr/pkg/lib/libGL.so"};
		int	     glincr   = 0;
		x11opengl_t* o = r = malloc(sizeof(*o));

		attribs[0] = GLX_RGBA;
		attribs[1] = GLX_DOUBLEBUFFER;
		attribs[2] = GLX_DEPTH_SIZE;
		attribs[3] = 24;
		attribs[4] = None;

		while(glpath[glincr] != NULL &&
		      (o->lib = MwDynamicOpen(glpath[glincr++])) == NULL);

		o->glXChooseVisual =
		    (MWglXChooseVisual)MwDynamicSymbol(o->lib, "glXChooseVisual");
		o->glXCreateContext =
		    (MWglXCreateContext)MwDynamicSymbol(o->lib, "glXCreateContext");
		o->glXDestroyContext =
		    (MWglXDestroyContext)MwDynamicSymbol(o->lib, "glXDestroyContext");
		o->glXMakeCurrent =
		    (MWglXMakeCurrent)MwDynamicSymbol(o->lib, "glXMakeCurrent");
		o->glXSwapBuffers =
		    (MWglXSwapBuffers)MwDynamicSymbol(o->lib, "glXSwapBuffers");
		o->glXGetProcAddress =
		    (MWglXGetProcAddress)MwDynamicSymbol(o->lib, "glXGetProcAddress");

		/* XXX: fix this */
		o->visual = o->glXChooseVisual(handle->lowlevel->x11.display,
					       DefaultScreen(handle->lowlevel->x11.display),
					       attribs);
		o->gl	  = o->glXCreateContext(handle->lowlevel->x11.display, o->visual, NULL,
						GL_TRUE);
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		EGLint		 numConfigs;
		EGLint		 numConfigsMatched;
		EGLint		 majorVersion;
		EGLint		 minorVersion;
		EGLContext	 context;
		EGLSurface	 surface;
		EGLint		 fbAttribs[]	  = {EGL_SURFACE_TYPE,
						     EGL_WINDOW_BIT,
						     EGL_RENDERABLE_TYPE,
						     EGL_OPENGL_BIT,
						     EGL_RED_SIZE,
						     8,
						     EGL_GREEN_SIZE,
						     8,
						     EGL_BLUE_SIZE,
						     8,
						     EGL_DEPTH_SIZE,
						     24,
						     EGL_NONE};
		EGLint		 contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,
						     1,
						     EGL_CONTEXT_MAJOR_VERSION,
						     1,
						     EGL_CONTEXT_MINOR_VERSION,
						     1,
						     EGL_NONE};
		EGLDisplay	 display;
		waylandopengl_t* o = r	    = malloc(sizeof(*o));
		int		 gbm_format = 0;
		EGLConfig	 egl_configs[1024];
		memset(o, 0, sizeof(waylandopengl_t));

		o->gllib = MwDynamicOpen("libEGL.so");
		if(!o->gllib) {
			printf("Could not load OpenGL widget under Wayland! libEGL.so missing.\n");
			return 1;
		}
		o->gbmlib = MwDynamicOpen("libgbm.so");
		if(!o->gbmlib) {
			printf("Could not load OpenGL widget under Wayland! libgbm.so missing.\n");
			return 1;
		}
#define EGL_FUNC(x) \
	o->x = MwDynamicSymbol(o->gllib, #x); \
	if(!o->x) { \
		printf("Could not load OpenGL widget under Wayland! " #x " missing.\n"); \
		return 1; \
	};
#define GBM_FUNC(x) \
	o->x = MwDynamicSymbol(o->gbmlib, #x); \
	if(!o->x) { \
		printf("Could not load OpenGL widget under Wayland! " #x " missing.\n"); \
		return 1; \
	};
		EGL_FUNC(eglGetConfigs)
		EGL_FUNC(eglInitialize)
		EGL_FUNC(eglCreateContext)
		EGL_FUNC(eglMakeCurrent)
		EGL_FUNC(eglGetProcAddress)
		EGL_FUNC(eglSwapBuffers)
		EGL_FUNC(eglSwapInterval)
		EGL_FUNC(eglGetError)
		EGL_FUNC(eglBindAPI)
		EGL_FUNC(eglGetDisplay)
		EGL_FUNC(eglCreatePlatformWindowSurface)
		EGL_FUNC(eglChooseConfig)
		EGL_FUNC(eglGetPlatformDisplay)
		EGL_FUNC(eglGetConfigAttrib)
		EGL_FUNC(eglDestroySurface)
		GBM_FUNC(gbm_create_device)
		GBM_FUNC(gbm_surface_create)
		GBM_FUNC(gbm_surface_destroy)
		GBM_FUNC(gbm_surface_lock_front_buffer)
		GBM_FUNC(gbm_bo_get_handle)
		GBM_FUNC(gbm_bo_get_stride)
		GBM_FUNC(gbm_bo_get_width)
		GBM_FUNC(gbm_bo_get_height)
		GBM_FUNC(gbm_surface_release_buffer)
		GBM_FUNC(gbm_bo_map)
		GBM_FUNC(gbm_bo_unmap)

#define eglGetConfigs o->eglGetConfigs
#define eglInitialize o->eglInitialize
#define eglCreateContext o->eglCreateContext
#define eglMakeCurrent o->eglMakeCurrent
#define eglGetProcAddress o->eglGetProcAddress
#define eglSwapBuffers o->eglSwapBuffers
#define eglSwapInterval o->eglSwapInterval
#define eglGetError o->eglGetError
#define eglBindAPI o->eglBindAPI
#define eglGetDisplay o->eglGetDisplay
#define eglCreatePlatformWindowSurface o->eglCreatePlatformWindowSurface
#define eglDestroySurface o->eglDestroySurface
#define eglChooseConfig o->eglChooseConfig
#define eglGetPlatformDisplay o->eglGetPlatformDisplay

		o->drm_fd = open("/dev/dri/renderD128", O_RDWR);
		assert(o->drm_fd > 0);
		o->gbm = o->gbm_create_device(o->drm_fd);
		assert(o->gbm != NULL);

		display = eglGetPlatformDisplay(0x31D7, o->gbm, NULL);
		if(display == EGL_NO_DISPLAY) {
			printf("ERROR: eglGetDisplay, %0X\n", eglGetError());
			return 1;
		}

		/* Initialize EGL */
		if(!eglInitialize(display, &majorVersion, &minorVersion)) {
			printf("ERROR: eglInitialize, %0X\n", eglGetError());
			return 1;
		}

		eglBindAPI(EGL_OPENGL_API);

		/* Get configs */
		if((eglGetConfigs(display, NULL, 0, &numConfigs) != EGL_TRUE)) {
			printf("ERROR: eglGetConfigs, %0X\n", eglGetError());
			return 1;
		}

		/* Choose config */
		if((eglChooseConfig(display, fbAttribs, egl_configs, numConfigs, &numConfigsMatched) !=
		    EGL_TRUE)) {
			printf("ERROR: eglChooseConfig, %0X\n", eglGetError());
			return 1;
		}

		for(int i = 0; i < numConfigsMatched; i++) {
			if(o->eglGetConfigAttrib(display, egl_configs[i],
						 EGL_NATIVE_VISUAL_ID,
						 &gbm_format) == EGL_FALSE) {
				printf("ERROR: eglGetConfigAttrib, %0X\n", eglGetError());
			} else {
				if(gbm_format == GBM_FORMAT_ARGB8888) {
					o->egl_config = egl_configs[i];
					break;
				}
			}
		}

		/* Create a GL context */
		context = eglCreateContext(display, o->egl_config, EGL_NO_CONTEXT,
					   contextAttribs);
		if(context == EGL_NO_CONTEXT) {
			printf("ERROR: eglCreateContext, %0X\n", eglGetError());
			return 1;
		}

		o->gbm_surface = o->gbm_surface_create(o->gbm, handle->lowlevel->wayland.ww, handle->lowlevel->wayland.wh, GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);

		/* Create a surface */
		surface = eglCreatePlatformWindowSurface(display, o->egl_config,
							 o->gbm_surface, NULL);
		if(surface == EGL_NO_SURFACE) {
			printf("ERROR: eglCreatePlatformWindowSurface, %0X\n", eglGetError());
			return 1;
		}

		if(!eglMakeCurrent(display, surface, surface, context)) {
			printf("ERROR: eglMakeCurrent (setup): %0X\n", eglGetError());
		}

		o->frontbuffer_data = malloc(handle->lowlevel->wayland.ww * handle->lowlevel->wayland.wh * 4);
		memset(o->frontbuffer_data, 255, handle->lowlevel->wayland.ww * handle->lowlevel->wayland.wh * 4);

		o->frontbuffer = MwLoadRaw(handle, o->frontbuffer_data, handle->lowlevel->wayland.ww, handle->lowlevel->wayland.wh);

		MwVaApply(handle, MwNpixmap, o->frontbuffer, NULL);

		o->egl_display = display;
		o->egl_surface = surface;
		o->egl_context = context;
	}
#endif

	handle->internal		     = r;
	handle->lowlevel->common.copy_buffer = 0;

	MwSetDefault(handle);

	while(w->parent != NULL)
		w = w->parent;

	w->berserk++;

	return 0;
}

static void destroy(MwWidget handle) {
	MwWidget w = handle;

#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		gdiopengl_t* o = handle->internal;

		o->wglMakeCurrent(NULL, NULL);
		DeleteDC(o->dc);
		o->wglDeleteContext(o->gl);

		MwDynamicClose(o->lib);
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		x11opengl_t* o = handle->internal;

		o->glXMakeCurrent(handle->lowlevel->x11.display, None, NULL);
		o->glXDestroyContext(handle->lowlevel->x11.display, o->gl);

		MwDynamicClose(o->lib);
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		/* todo */
	}
#endif

	while(w->parent != NULL)
		w = w->parent;

	w->berserk--;

	free(handle->internal);
}

static void mwOpenGLMakeCurrentImpl(MwWidget handle) {
	/* these swap interval functions belonging here actually stink! */

#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		gdiopengl_t* o				= handle->internal;
		BOOL(APIENTRY * swap_interval_ext)(int) = NULL;

		o->wglMakeCurrent(o->dc, o->gl);

		if((swap_interval_ext =
			MwOpenGLGetProcAddress(handle, "wglSwapIntervalEXT")) != NULL) {
			swap_interval_ext(1);
		}
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		x11opengl_t* o					      = handle->internal;
		void (*swap_interval_ext)(Display*, GLXDrawable, int) = NULL;
		void (*swap_interval_mesa)(unsigned int)	      = NULL;
		void (*swap_interval_sgi)(int)			      = NULL;

		o->glXMakeCurrent(handle->lowlevel->x11.display,
				  handle->lowlevel->x11.window, o->gl);

		if((swap_interval_ext =
			MwOpenGLGetProcAddress(handle, "glXSwapIntervalEXT")) != NULL) {
			swap_interval_ext(handle->lowlevel->x11.display,
					  handle->lowlevel->x11.window, 1);
		}

		if((swap_interval_mesa =
			MwOpenGLGetProcAddress(handle, "glXSwapIntervalMESA")) != NULL) {
			swap_interval_mesa(1);
		}

		if((swap_interval_sgi =
			MwOpenGLGetProcAddress(handle, "glXSwapIntervalSGI")) != NULL) {
			swap_interval_sgi(1);
		}
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		waylandopengl_t* o = handle->internal;

		if(!eglMakeCurrent(o->egl_display, o->egl_surface, o->egl_surface,
				   o->egl_context)) {
			printf("ERROR: eglMakeCurrent, %0X\n", eglGetError());
		}

		MwLLForceRender(handle->lowlevel);
	}
#endif
}

static void mwOpenGLSwapBufferImpl(MwWidget handle) {
#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		gdiopengl_t* o = handle->internal;

		SwapBuffers(o->dc);
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		x11opengl_t* o = handle->internal;

		o->glXSwapBuffers(handle->lowlevel->x11.display,
				  handle->lowlevel->x11.window);
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		waylandopengl_t* o = handle->internal;
		struct gbm_bo*	 bo;
		eglSwapInterval(o->egl_display, 0);
		if(!eglSwapBuffers(o->egl_display, o->egl_surface)) {
			printf("ERROR: eglSwapBuffers, %0X\n", eglGetError());
		}
		bo = o->gbm_surface_lock_front_buffer(o->gbm_surface);
		if(bo) {
			void* gbmBoMapData = NULL; // Needed for unmapping if gbm_bo_map is used
			MwU32 width	   = o->gbm_bo_get_width(bo);
			MwU32 height	   = o->gbm_bo_get_height(bo);
			MwU32 stride	   = o->gbm_bo_get_stride(bo);
			void* map	   = o->gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_READ, &stride, &gbmBoMapData);
			MwU32 size	   = height * stride;

			if(map) {
				for(int y = 0; y < height; y++) {
					const MwU8* row = map + y * stride;
					for(int x = 0; x < width; x++) {
						/* Memory layout per pixel: [B][G][R][A] */
						MwU8 b = row[x * 4 + 0];
						MwU8 g = row[x * 4 + 1];
						MwU8 r = row[x * 4 + 2];

						int out			     = (y * width + x) * 4;
						o->frontbuffer_data[out + 0] = r;
						o->frontbuffer_data[out + 1] = g;
						o->frontbuffer_data[out + 2] = b;
						o->frontbuffer_data[out + 3] = 255;
					}
				}

				MwPixmapReloadRaw(
				    o->frontbuffer, o->frontbuffer_data);
			}
			o->gbm_bo_unmap(bo, gbmBoMapData);
		}
		if(o->previous_bo) {
			o->gbm_surface_release_buffer(o->gbm_surface, o->previous_bo);
		}
		o->previous_bo = bo;

		MwForceRender(handle);
	}
#endif
}

static void* mwOpenGLGetProcAddressImpl(MwWidget handle, const char* name) {
#ifdef USE_GDI
	if(handle->lowlevel->common.type == MwLLBackendGDI) {
		gdiopengl_t* o = handle->internal;

		return o->wglGetProcAddress(name);
	}
#endif
#ifdef USE_X11
	if(handle->lowlevel->common.type == MwLLBackendX11) {
		x11opengl_t* o = handle->internal;

		return o->glXGetProcAddress((const GLubyte*)name);
	}
#endif
#ifdef USE_WAYLAND
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		waylandopengl_t* o = handle->internal;
		return eglGetProcAddress(name);
	}
#endif
	return NULL;
}

static void func_handler(MwWidget handle, const char* name, void* out,
			 va_list va) {
	if(strcmp(name, "mwOpenGLMakeCurrent") == 0) {
		mwOpenGLMakeCurrentImpl(handle);
	}
	if(strcmp(name, "mwOpenGLSwapBuffer") == 0) {
		mwOpenGLSwapBufferImpl(handle);
	}
	if(strcmp(name, "mwOpenGLGetProcAddress") == 0) {
		const char* _name = va_arg(va, const char*);
		*(void**)out	  = mwOpenGLGetProcAddressImpl(handle, _name);
	}
}

#ifdef USE_WAYLAND
static void frontbuffer_draw(MwWidget handle) {
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		waylandopengl_t* o = handle->internal;
		MwRect		 r;
		r.x	 = 0;
		r.y	 = 0;
		r.width	 = handle->lowlevel->wayland.ww;
		r.height = handle->lowlevel->wayland.wh;
		MwLLDrawPixmap(handle->lowlevel, &r, o->frontbuffer);
	}
}
static void frontbuffer_resize(MwWidget handle) {
	if(handle->lowlevel->common.type == MwLLBackendWayland) {
		waylandopengl_t* o = handle->internal;
		if(o->frontbuffer_data) {
			free(o->frontbuffer_data);
		}
		if(o->frontbuffer) {
			MwLLDestroyPixmap(o->frontbuffer);
		}
		if(o->gbm_surface) {
			o->gbm_surface_destroy(o->gbm_surface);
		}
		if(o->egl_surface) {
			eglDestroySurface(o->egl_display, o->egl_surface);
		}

		o->gbm_surface = o->gbm_surface_create(o->gbm, handle->lowlevel->wayland.ww, handle->lowlevel->wayland.wh, GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);

		/* Create a surface */
		o->egl_surface = eglCreatePlatformWindowSurface(o->egl_display, o->egl_config,
								o->gbm_surface, NULL);
		if(o->egl_surface == EGL_NO_SURFACE) {
			printf("ERROR: eglCreatePlatformWindowSurface, %0X\n", eglGetError());
		} else {
		}

		if(!eglMakeCurrent(o->egl_display, o->egl_surface, o->egl_surface, o->egl_context)) {
			printf("ERROR: eglMakeCurrent (setup): %0X\n", eglGetError());
		}

		o->frontbuffer_data = malloc(handle->lowlevel->wayland.ww * handle->lowlevel->wayland.wh * 4);
		memset(o->frontbuffer_data, 255, handle->lowlevel->wayland.ww * handle->lowlevel->wayland.wh * 4);

		o->frontbuffer = MwLoadRaw(handle, o->frontbuffer_data, handle->lowlevel->wayland.ww, handle->lowlevel->wayland.wh);

		MwVaApply(handle, MwNpixmap, o->frontbuffer, NULL);
	}
}
#endif

MwClassRec MwOpenGLClassRec = {wcreate, /* create */
			       destroy, /* destroy */
#ifdef USE_WAYLAND
			       frontbuffer_draw, /* draw */
#else
			       NULL, /* draw */
#endif
			       NULL,	     /* click */
			       NULL,	     /* parent_resize */
			       NULL,	     /* prop_change */
			       NULL,	     /* mouse_move */
			       NULL,	     /* mouse_up */
			       NULL,	     /* mouse_down */
			       NULL,	     /* key */
			       func_handler, /* execute */
			       NULL,	     /* tick */
#ifdef USE_WAYLAND
			       frontbuffer_resize, /* resize */
#else
			       NULL, /* resize */
#endif
			       NULL, /* children_update */
			       NULL, /* children_prop_change */
			       NULL, /* clipboard */
			       NULL, /* props_change */
			       NULL, NULL, NULL};
MwClass MwOpenGLClass = &MwOpenGLClassRec;
#else
MWDECL MwClass MwOpenGLClass;

MwClass MwOpenGLClass = NULL;
#endif

#endif
