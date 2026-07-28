#include <Mw/Milsko.h>
#include <Threads.h>

#include "../../external/stb_ds.h"

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL r;
	r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	MwLLCreateCommon(r);

	r->cmacos.parent = parent;

	if(x == MwDEFAULT) {
		x = (qd.screenBits.bounds.right / 2) - (width / 2);
	}
	if(y == MwDEFAULT) {
		y = (qd.screenBits.bounds.bottom / 2) - (height / 2);
	}

	/* todo: how do we set parent windows? If we can't, what is our equivalant to MacOS's NSViews, Wayland's subsurfaces, etc.? */
	SetRect(&r->cmacos.winRect, x, y, width, height);
	r->cmacos.window = newcwindow(nil, &r->cmacos.winRect, "", true,
				      documentProc, (WindowPtr)(-1), true, 0);
	SetPort(r->cmacos.window);

	return r;
}

static void MwLLDestroyImpl(MwLL handle) {
	MwLLDestroyCommon(handle);

	free(handle);
}

static void MwLLBeginDrawImpl(MwLL handle) {
}

static void MwLLEndDrawImpl(MwLL handle) {
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count, MwLLColor color) {
	int	   i;
	PolyHandle p;
	RGBColor   col;

	SetPort(handle->cmacos.window);
	BeginUpdate(handle->cmacos.window);

	p	  = OpenPoly();
	col.red	  = color->common.red * 0x101;
	col.green = color->common.green * 0x101;
	col.blue  = color->common.blue * 0x101;

	for(i = 0; i < points_count; i++) {
		if(i == 0) {
			MoveTo(points[i].x, points[i].y);
		} else {
			LineTo(points[i].x, points[i].y);
		}
	}

	ClosePoly();

	PenMode(patCopy);
	RGBForeColor(&col);
	PaintPoly(p);

	KillPoly(p);

	EndUpdate(handle->cmacos.window);
}

static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	RGBColor col;
	SetPort(handle->cmacos.window);

	col.red	  = color->common.red;
	col.green = color->common.green;
	col.blue  = color->common.blue;

	RGBForeColor(&col);

	PenMode(patCopy);

	MoveTo(points[0].x, points[0].y);
	LineTo(points[1].x, points[1].y);
}

static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = malloc(sizeof(*c));
	MwLLColorUpdate(handle, c, r, g, b);
	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	c->common.red	= r;
	c->common.green = g;
	c->common.blue	= b;
}

static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) {
	Rect bounds;
	GetWindowBounds(handle->cmacos.window, kWindowContentRgn, &bounds);
	*x = bounds.left;
	*y = bounds.top;
	*w = bounds.right;
	*h = bounds.bottom;
}

static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	MoveWindow(handle->cmacos.window, x, y, MwTRUE);
}

static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	SizeWindow(handle->cmacos.window, w, h, MwTRUE);
}

static void MwLLFreeColorImpl(MwLLColor color) {
	free(color);
}

static int MwLLPendingImpl(MwLL handle) {
	YieldToAnyThread();
	return EventAvail(everyEvent, &handle->cmacos.event);
}

static void MwLLNextEventImpl(MwLL handle) {
	EventRecord event;
	GetNextEvent(everyEvent, &event);

	switch(event.what) {
	case updateEvt:
	{
		/*
		    This is our blocker for getting anything running.
		    This needs to be called at this point to start drawing anything.
		    ...It locks up and/or crashes the entire system. At least on SheepShaver.
		    It's not a stack overflow, it's not anything in the drawing functions.
			It's *something* else. But because Classic Mac OS development is like
			digging up forbidden tombs while stabbing yourself in the eye several times,
			I don't know what.

			Fun fact: for as much as everyone loves to glaze about AI being the future
			of programming, even Claude got confused when I gave up and tried to ask it
			for help. Information about Classic Mac OS probably takes up 0.0001% of its
			knowledge base; god knows it's hard enough finding information online about
			this!
		    */
		MwLLDispatch(handle, draw, NULL);
	} break;
	case osEvt:
		break;
	case mouseUp:
	{
		// MwLLMouse m;
		// m.button = MwLLMouseLeft;
		// if(___curWindow->mouseButtonCallback != NULL) {
		// 	int mods = MacModifiersToGLFWModifiers(event.modifiers);

		// 	if(___curWindow->keyStateMap[MK_LCTRL] == 1) {
		// 		___curWindow->mouseButtonCallback(
		// 		    ___curWindow, GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, mods);
		// 	} else {
		// 		___curWindow->mouseButtonCallback(
		// 		    ___curWindow, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, mods);
		// 	}
		// }
	}
	case mouseDown:
	{
		// // start with callback stuff
		// if(___curWindow->mouseButtonCallback != NULL) {
		// 	int mods = MacModifiersToGLFWModifiers(event.modifiers);

		// 	if(___curWindow->keyStateMap[MK_LCTRL] == 1) {
		// 		___curWindow->mouseButtonCallback(
		// 		    ___curWindow, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, mods);
		// 	} else {
		// 		___curWindow->mouseButtonCallback(
		// 		    ___curWindow, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, mods);
		// 	}
		// }

		// then do system stuff
		switch(FindWindow(event.where, &handle->cmacos.window)) {
		case inDrag:
			DragWindow(handle->cmacos.window, event.where, &qd.screenBits.bounds);
			// if(___curWindow->windowPosCallback != NULL) {
			// 	___curWindow->windowPosCallback(___curWindow, event.where.h,
			// 					event.where.v);
			// }
			break;
		case inGrow:
		{
			long growResult =
			    GrowWindow(handle->cmacos.window, event.where, &qd.screenBits.bounds);

			int width  = growResult & 0xFFFF;
			int height = growResult >> 16;

			SizeWindow(handle->cmacos.window, growResult & 0xFFFF, growResult >> 16, true);

			Rect rect;
			GetWindowBounds(handle->cmacos.window, kWindowStructureRgn, &rect);
			rect.right  = width;
			rect.bottom = height;
			SetWindowBounds(handle->cmacos.window, kWindowStructureRgn, &rect);

			// ___curWindow->width  = width;
			// ___curWindow->height = height;

			// if(___curWindow->windowSizeCallback != NULL) {
			// 	___curWindow->windowSizeCallback(___curWindow, width, height);
			// }
			// if(___curWindow->windowSetFramebufferSizeCallback != NULL) {
			// 	___curWindow->windowSetFramebufferSizeCallback(___curWindow,
			// 						       width, height);
			// }
		} break;

		case inGoAway:
		{
			if(TrackGoAway(handle->cmacos.window, event.where)) {
				MwLLDispatch(handle, close, NULL);
				// if(___curWindow->windowCloseCallback != NULL) {
				// ___curWindow->windowCloseCallback(___curWindow);
				// }
				// ___curWindow->shouldClose = true;
			}
		} break;
		}
	}
	case autoKey:
	case keyDown:
	case keyUp:
	{
		// // key char stuff
		// unsigned char ch = (event.message & charCodeMask);
		// if(*___curWindow->charCallback != NULL) {
		// 	___curWindow->charCallback(___curWindow, ch);
		// }

		// // key state map
		// unsigned char key	       = (event.message & keyCodeMask) >> 8;
		// ___curWindow->keyStateMap[key] = 0;
		// if(event.what == keyDown || event.what == autoKey) {
		// 	___curWindow->keyStateMap[key] &= 1;
		// }

		// if(*___curWindow->keyCallback != NULL) {
		// 	int key	     = MacKeyToGLFWKey(event.message);
		// 	int scancode = (event.message & adbAddrMask) >> 16;

		// 	int mods = MacModifiersToGLFWModifiers(event.modifiers);

		// 	int action;
		// 	if(event.what == keyUp) {
		// 		action = GLFW_RELEASE;
		// 	} else {
		// 		action = GLFW_PRESS;
		// 	}
		// 	___curWindow->keyCallback(___curWindow, key, scancode, action,
		// 				  mods);
		// }
	}
	}

	FlushEvents(everyEvent, -1);
	YieldToAnyThread();
}

static void MwLLSetTitleImpl(MwLL handle, const char* title) {
	setwtitle(handle->cmacos.window, title);
}

static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data, int width, int height) {
	MwLLPixmap r = malloc(sizeof(*r));

	r->common.raw = malloc(4 * width * height);
	memcpy(r->common.raw, data, 4 * width * height);

	MwLLPixmapUpdate(r);
	return r;
}

static void MwLLPixmapUpdateImpl(MwLLPixmap r) {
}

static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
}

static void MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {
}

static void MwLLForceRenderImpl(MwLL handle) {
}

static void MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {
}

static void MwLLDetachImpl(MwLL handle, MwPoint* point) {
}

static void MwLLShowImpl(MwLL handle, int show) {
	if(show)
		ShowWindow(handle->cmacos.window);
	else
		HideWindow(handle->cmacos.window);
}

static void MwLLMakePopupImpl(MwLL handle, MwLL parent) {
}

static void MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx, int maxy) {
}

static void MwLLMakeBorderlessImpl(MwLL handle, int toggle) {
}

static void MwLLFocusImpl(MwLL handle) {
	HiliteWindow(handle->cmacos.window, MwTRUE);
}

static void MwLLGrabPointerImpl(MwLL handle, int toggle) {
}

static void MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {
	/* TODO */

	(void)handle;
	(void)text;
}

static void MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {
}

static void MwLLMakeToolWindowImpl(MwLL handle) {
}

static void MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {
}

static void MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {
}

static void MwLLBeginStateChangeImpl(MwLL handle) {
	MwLLShow(handle, 0);
}

static void MwLLEndStateChangeImpl(MwLL handle) {
	MwLLShow(handle, 1);
}

static void MwLLSetDarkThemeImpl(MwLL handle, int toggle) {
	(void)handle;
	(void)toggle;
}

static MwBool MwLLDoModernImpl(MwLL handle) {
	(void)handle;
	return MwFALSE;
}

static void MwLLRaiseImpl(MwLL handle) {
	(void)handle;
}

static void MwLLClipImpl(MwLL handle, MwRect* rect) {
	(void)handle;
	(void)rect;
}

static int MwLLClassicMacOSCallInitImpl(void) {
	InitGraf(&qd.thePort);
	InitFonts();
	FlushEvents(everyEvent, 0);
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(0L);
	InitCursor();

	return 0;
}

#include "call.c"
CALL(ClassicMacOS);
