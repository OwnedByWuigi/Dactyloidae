#include "Mw/BaseTypes.h"
#include "Mw/Core.h"
#include "Mw/LowLevel/Cocoa.h"
#include "Mw/StringDefs.h"
#include <Mw/Milsko.h>
#include <Mw/Widget/OpenGL.h>
#import <mach-o/dyld.h>

@interface MacOpenGLWidget : NSObject {
	NSOpenGLPixelFormat* pixelFormat;
	NSOpenGLContext*     glc;
	MilskoCocoa*	     _win;
}

- (MacOpenGLWidget*)initWithWindow:(MilskoCocoa*)w;
- (void)destroy;
- (void)makeCurrent;
- (void)swapBuffer;
- (void*)getProcAddressWithName:(const char*)name;

@end

static int wcreate(MwWidget handle) {
	void*	 r = NULL;
	MwWidget w = handle;

	int x	   = MwGetInteger(w, MwNx);
	int y	   = MwGetInteger(w, MwNy);
	int width  = MwGetInteger(w, MwNwidth);
	int height = MwGetInteger(w, MwNheight);

	printf("%d %d\n", width, height);

	handle->internal =
	    [[MacOpenGLWidget alloc] initWithWindow:handle->lowlevel->cocoa.real];
	handle->lowlevel->common.copy_buffer = 0;

	MwSetDefault(handle);

	while(w->parent != NULL)
		w = w->parent;

	w->berserk++;

	return 0;
}

static void destroy(MwWidget handle) {
	[(MacOpenGLWidget*)handle->internal destroy];
}

static void mwOpenGLMakeCurrentImpl(MwWidget handle) {
	[(MacOpenGLWidget*)handle->internal makeCurrent];
}

static void mwOpenGLSwapBufferImpl(MwWidget handle) {
	[(MacOpenGLWidget*)handle->internal swapBuffer];
}

static void* mwOpenGLGetProcAddressImpl(MwWidget handle, const char* name) {
	return [(MacOpenGLWidget*)handle->internal getProcAddressWithName:name];
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

@implementation MacOpenGLWidget

- (MacOpenGLWidget*)initWithWindow:(MilskoCocoa*)w {
	NSOpenGLPixelFormatAttribute pixelFormatAttributes[] = {
	    NSOpenGLPFAColorSize, 24,
	    NSOpenGLPFAStencilSize, 8,
	    NSOpenGLPFAAlphaSize, 8,
	    NSOpenGLPFADoubleBuffer, NSOpenGLPFAAccelerated,
	    NSOpenGLPFANoRecovery, 0};
	self->pixelFormat =
	    [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelFormatAttributes];
	self->glc =
	    [[NSOpenGLContext alloc] initWithFormat:pixelFormat
				       shareContext:nil];
	[self->glc setView:[w getView]];
	self->_win = w;
	return self;
};
- (void)destroy {
};
- (void)makeCurrent {
	[self->glc makeCurrentContext];
};
- (void)swapBuffer {
	[self->glc flushBuffer];
};
- (void*)getProcAddressWithName:(const char*)name {
	if(NSIsSymbolNameDefined(name)) {
		return NSAddressOfSymbol(NSLookupAndBindSymbol(name));
	} else {
		return NULL;
	}
};

@end

MwClassRec MwOpenGLClassRec = {wcreate,	     /* create */
			       destroy,	     /* destroy */
			       NULL,	     /* draw */
			       NULL,	     /* click */
			       NULL,	     /* parent_resize */
			       NULL,	     /* prop_change */
			       NULL,	     /* mouse_move */
			       NULL,	     /* mouse_up */
			       NULL,	     /* mouse_down */
			       NULL,	     /* key */
			       func_handler, /* execute */
			       NULL,	     /* tick */
			       NULL,	     /* resize */
			       NULL,	     /* children_update */
			       NULL,	     /* children_prop_change */
			       NULL,	     /* clipboard */
			       NULL, NULL, NULL, NULL};
MwClass	   MwOpenGLClass    = &MwOpenGLClassRec;
