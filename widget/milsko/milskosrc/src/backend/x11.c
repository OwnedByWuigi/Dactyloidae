#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

#define ClipboardTimeout 100

#ifdef USE_XRENDER
// #undef USE_XRENDER
#endif

static struct symtbl {
	void*  lib_xlib;
	void*  lib_xrender;
	MwBool has_xrender;

#ifdef USE_DBUS
	MwLLDBusFuncTable dbus;
	MwBool		  has_dbus;
#endif

	int (*XClearWindow)(Display* display, Window w);
	XImage* (*XCreateImage)(Display*, Visual*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int);
	Display* (*XOpenDisplay)(_Xconst char*);
	char* (*XKeysymToString)(KeySym);
	Atom (*XInternAtom)(Display*, _Xconst char*, Bool);
	Cursor (*XCreatePixmapCursor)(Display*, Pixmap, Pixmap, XColor*, XColor*, unsigned int, unsigned int);
	GC (*XCreateGC)(Display*, Drawable, unsigned long, XGCValues*);
	Pixmap (*XCreatePixmap)(Display*, Drawable, unsigned int, unsigned int, unsigned int);
	Window (*XCreateSimpleWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long);
	VisualID (*XVisualIDFromVisual)(Visual*);
	Window (*XRootWindow)(Display*, int);
	Status (*XSetWMProtocols)(Display*, Window, Atom*, int);
	int (*XSetTransientForHint)(Display*, Window, Window);
	Status (*XAllocColor)(Display*, Colormap, XColor*);
	int (*XChangeProperty)(Display*, Window, Atom, Atom, int, int, _Xconst unsigned char*, int);
	int (*XChangeWindowAttributes)(Display*, Window, unsigned long, XSetWindowAttributes*);
	Bool (*XCheckTypedWindowEvent)(Display*, Window, int, XEvent*);
	Bool (*XCheckWindowEvent)(Display*, Window, long, XEvent*);
	int (*XCloseDisplay)(Display*);
	int (*XConfigureWindow)(Display*, Window, unsigned int, XWindowChanges*);
	int (*XConvertSelection)(Display*, Atom, Atom, Atom, Window, Time);
	int (*XCopyArea)(Display*, Drawable, Drawable, GC, int, int, unsigned int, unsigned int, int, int);
	int (*XDefaultScreen)(Display*);
	int (*XDefineCursor)(Display*, Window, Cursor);
	int (*XDeleteProperty)(Display*, Window, Atom);
	int (*XDestroyWindow)(Display*, Window);
	int (*XDrawLine)(Display*, Drawable, GC, int, int, int, int);
	int (*XFillPolygon)(Display*, Drawable, GC, XPoint*, int, int, int);
	int (*XFree)(void*);
	int (*XFreeCursor)(Display*, Cursor);
	int (*XFreeGC)(Display*, GC);
	int (*XFreePixmap)(Display*, Pixmap);
	Status (*XGetGeometry)(Display*, Drawable, Window*, int*, int*, unsigned int*, unsigned int*, unsigned int*, unsigned int*);
	int (*XGetWindowProperty)(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long*, unsigned long*, unsigned char**);
	Status (*XGetWindowAttributes)(Display*, Window, XWindowAttributes*);
	int (*XMapWindow)(Display*, Window);
	int (*XNextEvent)(Display*, XEvent*);
	int (*XPending)(Display*);
	int (*XPutBackEvent)(Display*, XEvent*);
	int (*XPutImage)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned int, unsigned int);
	Bool (*XQueryPointer)(Display*, Window, Window*, Window*, int*, int*, int*, int*, unsigned int*);
	Status (*XQueryTree)(Display*, Window, Window*, Window*, Window**, unsigned int*);
	int (*XReparentWindow)(Display*, Window, Window, int, int);
	int (*XSelectInput)(Display*, Window, long);
	Status (*XSendEvent)(Display*, Window, Bool, long, XEvent*);
	int (*XSetClipMask)(Display*, GC, Pixmap);
	int (*XSetClipOrigin)(Display*, GC, int, int);
	int (*XSetClipRectangles)(Display*, GC, int, int, XRectangle[], int, int);
	int (*XSetForeground)(Display*, GC, unsigned long);
	int (*XSetGraphicsExposures)(Display*, GC, Bool);
	int (*XSetInputFocus)(Display*, Window, int, Time);
	int (*XSetWindowBackgroundPixmap)(Display*, Window, Pixmap);
	int (*XSync)(Display*, Bool);
	Bool (*XTranslateCoordinates)(Display*, Window, Window, int, int, int*, int*, Window*);
	int (*XUnmapWindow)(Display*, Window);
	int (*XWarpPointer)(Display*, Window, Window, int, int, unsigned int, unsigned int, int, int);
	char* (*XSetLocaleModifiers)(const char*);
	XIM (*XOpenIM)(Display*, struct _XrmHashBucketRec*, char*, char*);
	Status (*XCloseIM)(XIM);
	XIC (*XCreateIC)(XIM, ...);
	void (*XDestroyIC)(XIC);
	void (*XSetICFocus)(XIC);
	XImage* (*XGetImage)(Display*, Drawable, int, int, unsigned int, unsigned int, unsigned long, int);
	int (*XRaiseWindow)(Display*, Window);

	XSizeHints* (*XAllocSizeHints)(void);
	XVisualInfo* (*XGetVisualInfo)(Display*, long, XVisualInfo*, int*);
	Status (*XGetWMNormalHints)(Display*, Window, XSizeHints*, long*);
	Status (*XGetWMSizeHints)(Display*, Window, XSizeHints*, long*, Atom);
	int (*XLookupString)(XKeyEvent*, char*, int, KeySym*, XComposeStatus*);
	int (*XSetStandardProperties)(Display*, Window, _Xconst char*, _Xconst char*, Pixmap, char**, int, XSizeHints*);
	void (*XSetWMNormalHints)(Display*, Window, XSizeHints*);
	void (*XSetWMSizeHints)(Display*, Window, XSizeHints*, Atom);

#if USE_XRENDER
	Bool (*XRenderQueryExtension)(Display* dpy, int* event_basep, int* error_basep);

	XRenderPictFormat* (*XRenderFindStandardFormat)(Display* dpy, int format);

	Picture (*XRenderCreatePicture)(Display* dpy, Drawable drawable,
					_Xconst XRenderPictFormat*	  format,
					unsigned long			  valuemask,
					_Xconst XRenderPictureAttributes* attributes);

	void (*XRenderFreePicture)(Display* dpy, Picture picture);

	void (*XRenderSetPictureTransform)(Display* dpy, Picture picture,
					   XTransform* transform);

	void (*XRenderComposite)(Display* dpy, int op, Picture src, Picture mask,
				 Picture dst, int src_x, int src_y, int mask_x, int mask_y,
				 int dst_x, int dst_y, unsigned int width,
				 unsigned int height);
#endif
} xsymtbl;

#define XClearWindow xsymtbl.XClearWindow
#define XCreateIC xsymtbl.XCreateIC
#define XFree xsymtbl.XFree
#define XFreeCursor xsymtbl.XFreeCursor
#define XConfigureWindow xsymtbl.XConfigureWindow
#define XCheckTypedWindowEvent xsymtbl.XCheckTypedWindowEvent
#define XGetWMNormalHints xsymtbl.XGetWMNormalHints
#define XLookupString xsymtbl.XLookupString
#define XCreateImage xsymtbl.XCreateImage
#define XSetClipMask xsymtbl.XSetClipMask
#define XConvertSelection xsymtbl.XConvertSelection
#define XGetWindowProperty xsymtbl.XGetWindowProperty
#define XSetWMNormalHints xsymtbl.XSetWMNormalHints
#define XTranslateCoordinates xsymtbl.XTranslateCoordinates
#define XCreateSimpleWindow xsymtbl.XCreateSimpleWindow
#define XGetWMSizeHints xsymtbl.XGetWMSizeHints
#define XRootWindow xsymtbl.XRootWindow
#define XOpenDisplay xsymtbl.XOpenDisplay
#define XCopyArea xsymtbl.XCopyArea
#define XDeleteProperty xsymtbl.XDeleteProperty
#define XDrawLine xsymtbl.XDrawLine
#define XSetGraphicsExposures xsymtbl.XSetGraphicsExposures
#define XDestroyWindow xsymtbl.XDestroyWindow
#define XCloseDisplay xsymtbl.XCloseDisplay
#define XSetForeground xsymtbl.XSetForeground
#define XGetGeometry xsymtbl.XGetGeometry
#define XUnmapWindow xsymtbl.XUnmapWindow
#define XSetTransientForHint xsymtbl.XSetTransientForHint
#define XSetWMSizeHints xsymtbl.XSetWMSizeHints
#define XCreatePixmapCursor xsymtbl.XCreatePixmapCursor
#define XFreeGC xsymtbl.XFreeGC
#define XNextEvent xsymtbl.XNextEvent
#define XSetWMProtocols xsymtbl.XSetWMProtocols
#define XDefaultScreen xsymtbl.XDefaultScreen
#define XKeysymToString xsymtbl.XKeysymToString
#define XMapWindow xsymtbl.XMapWindow
#define XPutBackEvent xsymtbl.XPutBackEvent
#define XCreateGC xsymtbl.XCreateGC
#define XSelectInput xsymtbl.XSelectInput
#define XSetInputFocus xsymtbl.XSetInputFocus
#define XDestroyIC xsymtbl.XDestroyIC
#define XSync xsymtbl.XSync
#define XSendEvent xsymtbl.XSendEvent
#define XSetICFocus xsymtbl.XSetICFocus
#define XWarpPointer xsymtbl.XWarpPointer
#define XPutImage xsymtbl.XPutImage
#define XFillPolygon xsymtbl.XFillPolygon
#define XOpenIM xsymtbl.XOpenIM
#define XDefineCursor xsymtbl.XDefineCursor
#define XSetWindowBackgroundPixmap xsymtbl.XSetWindowBackgroundPixmap
#define XAllocColor xsymtbl.XAllocColor
#define XInternAtom xsymtbl.XInternAtom
#define XQueryPointer xsymtbl.XQueryPointer
#define XGetVisualInfo xsymtbl.XGetVisualInfo
#define XVisualIDFromVisual xsymtbl.XVisualIDFromVisual
#define XSetLocaleModifiers xsymtbl.XSetLocaleModifiers
#define XGetWindowAttributes xsymtbl.XGetWindowAttributes
#define XSetStandardProperties xsymtbl.XSetStandardProperties
#define XChangeProperty xsymtbl.XChangeProperty
#define XAllocSizeHints xsymtbl.XAllocSizeHints
#define XCheckWindowEvent xsymtbl.XCheckWindowEvent
#define XFreePixmap xsymtbl.XFreePixmap
#define XReparentWindow xsymtbl.XReparentWindow
#define XCreatePixmap xsymtbl.XCreatePixmap
#define XSetClipOrigin xsymtbl.XSetClipOrigin
#define XSetClipRectangles xsymtbl.XSetClipRectangles
#define XCloseIM xsymtbl.XCloseIM
#define XQueryTree xsymtbl.XQueryTree
#define XPending xsymtbl.XPending
#define XChangeWindowAttributes xsymtbl.XChangeWindowAttributes
#define XGetImage xsymtbl.XGetImage
#define XRaiseWindow xsymtbl.XRaiseWindow

#if USE_XRENDER
#define XRenderFindStandardFormat xsymtbl.XRenderFindStandardFormat
#define XRenderCreatePicture xsymtbl.XRenderCreatePicture
#define XRenderFreePicture xsymtbl.XRenderFreePicture
#define XRenderComposite xsymtbl.XRenderComposite
#define XRenderQueryExtension xsymtbl.XRenderQueryExtension
#define XRenderSetPictureTransform xsymtbl.XRenderSetPictureTransform
#endif

typedef struct mwm_hints {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long	      input_mode;
	unsigned long status;
} mwm_hints_t;

enum mwm_hints_enum {
	MWM_HINTS_FUNCTIONS   = (1L << 0),
	MWM_HINTS_DECORATIONS = (1L << 1),
	MWM_FUNC_ALL	      = (1L << 0),
	MWM_FUNC_RESIZE	      = (1L << 1),
	MWM_FUNC_MOVE	      = (1L << 2),
	MWM_FUNC_MINIMIZE     = (1L << 3),
	MWM_FUNC_MAXIMIZE     = (1L << 4),
	MWM_FUNC_CLOSE	      = (1L << 5)
};

static unsigned long mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | KeymapNotify | FocusChangeMask;

static void create_pixmap(MwLL handle) {
	XWindowAttributes attr;
	int		  x, y;
	unsigned int	  w, h;

	MwLLGetXYWH(handle, &x, &y, &w, &h);

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &attr);

	handle->x11.pixmap = XCreatePixmap(handle->x11.display, handle->x11.window, w, h, attr.depth);
}

static void destroy_pixmap(MwLL handle) {
	XFreePixmap(handle->x11.display, handle->x11.pixmap);
}

static void sync_move(MwLL handle, int x, int y) {
	XEvent*		  queue = NULL;
	XEvent		  ev;
	unsigned long	  n = MwTimeGetTick() + 100;
	XWindowAttributes xwa;

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);

	XSync(handle->x11.display, False);

	while(!xwa.override_redirect && n > MwTimeGetTick()) {
		XSync(handle->x11.display, False);
		if(!XPending(handle->x11.display)) continue;
		XNextEvent(handle->x11.display, &ev);
		if(ev.type == ReparentNotify && ev.xreparent.window == handle->x11.window) {
			break;
		} else {
			arrput(queue, ev);
		}
	}

	while(arrlen(queue) > 0) {
		XPutBackEvent(handle->x11.display, &queue[0]);
		arrdel(queue, 0);
	}
	arrfree(queue);

	MwLLSetXY(handle, x, y);
}

static void wait_map(MwLL handle, int sync) {
	XWindowAttributes xwa;

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);
	if(xwa.map_state != IsViewable) {
		XSync(handle->x11.display, False);

		XMapWindow(handle->x11.display, handle->x11.window);
		XSync(handle->x11.display, False);

		if(!sync) return;
		do {
			XMapWindow(handle->x11.display, handle->x11.window);
			XSync(handle->x11.display, False);
			XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);
		} while(xwa.map_state != IsViewable);
	}
}

static void wait_unmap(MwLL handle, int sync) {
	XWindowAttributes xwa;

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);
	if(xwa.map_state != IsUnmapped) {
		XSync(handle->x11.display, False);

		XUnmapWindow(handle->x11.display, handle->x11.window);
		XSync(handle->x11.display, False);

		if(!sync) return;
		do {
			XUnmapWindow(handle->x11.display, handle->x11.window);
			XSync(handle->x11.display, False);
			XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);
		} while(xwa.map_state != IsUnmapped);
	}
}

static unsigned long generate_color(MwLL handle, unsigned long r, unsigned long g, unsigned long b) {
	unsigned long c = 0;

	c |= (r * handle->x11.red_max / 255) << handle->x11.red_shift;

	c |= (g * handle->x11.green_max / 255) << handle->x11.green_shift;

	c |= (b * handle->x11.blue_max / 255) << handle->x11.blue_shift;

	return c;
}

static XVisualInfo* get_visual_info(Display* display) {
	XVisualInfo xvi;
	int	    n;
	Visual*	    visual = DefaultVisual(display, DefaultScreen(display));

	xvi.visualid = XVisualIDFromVisual(visual);

	return XGetVisualInfo(display, VisualIDMask, &xvi, &n);
}

#ifdef USE_DBUS
static void detect_dark_theme(MwLL handle) {
	MwU32 value	 = 0;
	MwU32 dark_theme = 0;
	MwLLDBusPortalGet(&xsymtbl.dbus, &handle->x11.dbus, "org.freedesktop.portal.Settings", "org.freedesktop.appearance", "color-scheme", &value);

	dark_theme = (value == 1) ? 1 : 0;

	MwLLDispatch(handle, dark_theme, &dark_theme);
}
#endif

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL	      r;
	Window	      p;
	XVisualInfo*  xvi;
	unsigned long n = 1;
	int	      i;
	int	      px = x, py = y;
	XSizeHints    sh;

	r = malloc(sizeof(*r));

	MwLLCreateCommon(r);

	if(px == MwDEFAULT) px = 0;
	if(py == MwDEFAULT) py = 0;
	if(width < 2) width = 2;
	if(height < 2) height = 2;

	if(parent == NULL) {
		r->x11.display	= XOpenDisplay(NULL);
		p		= XRootWindow(r->x11.display, XDefaultScreen(r->x11.display));
		r->x11.top	= 1;
		r->x11.toplevel = 1;
	} else {
		r->x11.display	= parent->x11.display;
		p		= parent->x11.window;
		r->x11.top	= 0;
		r->x11.toplevel = 0;
	}
	r->x11.window = XCreateSimpleWindow(r->x11.display, p, px, py, width, height, 0, 0, WhitePixel(r->x11.display, DefaultScreen(r->x11.display)));

	sh.flags       = PWinGravity;
	sh.win_gravity = StaticGravity;
	XSetWMNormalHints(r->x11.display, r->x11.window, &sh);

	xvi = get_visual_info(r->x11.display);

	if(xvi->red_mask != 0) {
		i = 0;
		while(!((n << i) & xvi->red_mask)) i++;
		r->x11.red_mask	 = xvi->red_mask;
		r->x11.red_max	 = xvi->red_mask >> i;
		r->x11.red_shift = i;

		i = 0;
		while(!((n << i) & xvi->green_mask)) i++;
		r->x11.green_mask  = xvi->green_mask;
		r->x11.green_max   = xvi->green_mask >> i;
		r->x11.green_shift = i;

		i = 0;
		while(!((n << i) & xvi->blue_mask)) i++;
		r->x11.blue_mask  = xvi->blue_mask;
		r->x11.blue_max	  = xvi->blue_mask >> i;
		r->x11.blue_shift = i;
	}

	XFree(xvi);

#ifdef DO_XIM
	XSetLocaleModifiers("");
	if((r->x11.xim = XOpenIM(r->x11.display, 0, 0, 0)) == NULL) {
		XSetLocaleModifiers("@im=none");
		r->x11.xim = XOpenIM(r->x11.display, 0, 0, 0);
	}

	r->x11.xic = XCreateIC(r->x11.xim,
			       XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			       XNClientWindow, r->x11.window,
			       XNFocusWindow, r->x11.window,
			       NULL);
	XSetICFocus(r->x11.xic);
#endif

	r->common.copy_buffer = 1;
	r->common.type	      = MwLLBackendX11;

	r->x11.width  = width;
	r->x11.height = height;

	r->x11.grabbed		    = 0;
	r->x11.force_render	    = 0;
	r->x11.dark_theme_detection = 0;

	r->x11.colormap	    = DefaultColormap(r->x11.display, XDefaultScreen(r->x11.display));
	r->x11.wm_delete    = XInternAtom(r->x11.display, "WM_DELETE_WINDOW", False);
	r->x11.wm_protocols = XInternAtom(r->x11.display, "WM_PROTOCOLS", False);
	XSetWMProtocols(r->x11.display, r->x11.window, &r->x11.wm_delete, 1);

	r->x11.utf8_string   = XInternAtom(r->x11.display, "UTF8_STRING", False);
	r->x11.compound_text = XInternAtom(r->x11.display, "COMPOUND_TEXT", False);
	r->x11.text	     = XInternAtom(r->x11.display, "TEXT", False);
	r->x11.clipboard     = XInternAtom(r->x11.display, "CLIPBOARD", False);
	r->x11.selection     = XInternAtom(r->x11.display, "_MILSKO_SELECTION_", False);

	r->x11.clipboard_pending = 0;

	r->x11.gc = XCreateGC(r->x11.display, r->x11.window, 0, NULL);

	create_pixmap(r);

	XSetGraphicsExposures(r->x11.display, r->x11.gc, False);

	XSelectInput(r->x11.display, r->x11.window, mask);

	wait_map(r, 0);

	if(x != MwDEFAULT || y != MwDEFAULT) {
		unsigned int dummy;

		MwLLGetXYWH(r, &px, &py, &dummy, &dummy);

		if(x == MwDEFAULT) x = px;
		if(y == MwDEFAULT) y = py;

		if(parent == NULL) {
			sync_move(r, x, y);
		} else {
			MwLLSetXY(r, x, y);
		}
	}

#ifdef USE_DBUS
	memset(&r->x11.dbus, 0, sizeof(r->x11.dbus));
	if(!parent && xsymtbl.has_dbus) {
		xsymtbl.has_dbus	    = MwLLDBusNewContext(&xsymtbl.dbus, &r->x11.dbus);
		r->x11.dark_theme_detection = MwTRUE;
	}
#endif

	return r;
}

static void MwLLDestroyImpl(MwLL handle) {
	MwLLDestroyCommon(handle);

#ifdef DO_XIM
	if(handle->x11.xic) XDestroyIC(handle->x11.xic);
	if(handle->x11.xim) XCloseIM(handle->x11.xim);
#endif

	destroy_pixmap(handle);
	XFreeGC(handle->x11.display, handle->x11.gc);
	XUnmapWindow(handle->x11.display, handle->x11.window);
	XDestroyWindow(handle->x11.display, handle->x11.window);

	XSync(handle->x11.display, False);

	if(handle->x11.toplevel) XCloseDisplay(handle->x11.display);

#ifdef USE_DBUS
	if(!xsymtbl.has_dbus) {
		MwLLDBusFreeContext(&xsymtbl.dbus, &handle->x11.dbus);
	}
#endif

	free(handle);
}

static void MwLLBeginDrawImpl(MwLL handle) {
	(void)handle;
}

static void MwLLEndDrawImpl(MwLL handle) {
	(void)handle;

	if(handle->common.copy_buffer) XClearWindow(handle->x11.display, handle->x11.window);
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count, MwLLColor color) {
	int	i;
	XPoint* p = malloc(sizeof(*p) * points_count);

	XSetForeground(handle->x11.display, handle->x11.gc, color->x11.pixel);

	for(i = 0; i < points_count; i++) {
		p[i].x = points[i].x;
		p[i].y = points[i].y;
	}
	XFillPolygon(handle->x11.display, handle->x11.pixmap, handle->x11.gc, p, points_count, Nonconvex, CoordModeOrigin);

	free(p);
}

static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	XSetForeground(handle->x11.display, handle->x11.gc, color->x11.pixel);

	XDrawLine(handle->x11.display, handle->x11.pixmap, handle->x11.gc, points[0].x, points[0].y, points[1].x, points[1].y);
}

static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = malloc(sizeof(*c));
	MwLLColorUpdate(handle, c, r, g, b);
	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	XColor xc;

	if(handle->x11.red_mask == 0) {
		if(r > 255) r = 255;
		if(g > 255) g = 255;
		if(b > 255) b = 255;
		if(r < 0) r = 0;
		if(g < 0) g = 0;
		if(b < 0) b = 0;

		xc.red	 = 256 * r;
		xc.green = 256 * g;
		xc.blue	 = 256 * b;
		XAllocColor(handle->x11.display, handle->x11.colormap, &xc);

		c->x11.pixel = xc.pixel;
	} else {
		c->x11.pixel = generate_color(handle, r, g, b);
	}
	c->common.red	= r;
	c->common.green = g;
	c->common.blue	= b;
}

static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) {
	Window	     root;
	unsigned int border, depth;

	XGetGeometry(handle->x11.display, handle->x11.window, &root, x, y, w, h, &border, &depth);
	if(handle->x11.top) {
		int    rx, ry;
		Window child;

		XTranslateCoordinates(handle->x11.display, handle->x11.window, root, 0, 0, &rx, &ry, &child);

		*x = rx;
		*y = ry;
	}
}

static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	XSizeHints	  sh;
	long		  r;
	XWindowChanges	  xwc;
	XWindowAttributes xwa;

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);
	if(xwa.x == x && xwa.y == y) return;

	sh.flags = 0;
	XGetWMNormalHints(handle->x11.display, handle->x11.window, &sh, &r);

	sh.flags |= USPosition;
	sh.x = x;
	sh.y = y;

#if 1
	xwc.x = x;
	xwc.y = y;
	XConfigureWindow(handle->x11.display, handle->x11.window, CWX | CWY, &xwc);
#else
	XMoveWindow(handle->x11.display, handle->x11.window, x, y);
#endif
	XSetWMNormalHints(handle->x11.display, handle->x11.window, &sh);

	XSync(handle->x11.display, False);
}

static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	XSizeHints	  sh;
	long		  r;
	XWindowChanges	  xwc;
	XWindowAttributes xwa;

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);
	if(xwa.width == w && xwa.height == h) return;

	sh.flags = 0;
	XGetWMNormalHints(handle->x11.display, handle->x11.window, &sh, &r);

	if(w < 2) w = 2;
	if(h < 2) h = 2;

	sh.flags |= PSize;
	sh.width  = w;
	sh.height = h;

#if 1
	xwc.width  = w;
	xwc.height = h;
	XConfigureWindow(handle->x11.display, handle->x11.window, CWWidth | CWHeight, &xwc);
#else
	XResizeWindow(handle->x11.display, handle->x11.window, w, h);
#endif
	XSetWMNormalHints(handle->x11.display, handle->x11.window, &sh);

	destroy_pixmap(handle);
	create_pixmap(handle);

	/* we want events */
#if 0
	handle->x11.width  = w;
	handle->x11.height = h;
#endif

	XSync(handle->x11.display, False);

	MwLLForceRender(handle);
}

static void MwLLFreeColorImpl(MwLLColor color) {
	free(color);
}

static int MwLLPendingImpl(MwLL handle) {
	XEvent ev;

	if(handle->x11.dark_theme_detection) {
		handle->x11.dark_theme_detection = MwFALSE;
#ifdef USE_DBUS
		if(xsymtbl.has_dbus) {
			detect_dark_theme(handle);
			MwLLDispatch(handle, draw, NULL);
		}
#endif
	}

	if(handle->x11.clipboard_pending && (MwTimeGetTick() - handle->x11.clipboard_time) >= ClipboardTimeout) {
		return 1;
	}

	if(XCheckTypedWindowEvent(handle->x11.display, handle->x11.window, ClientMessage, &ev) || XCheckTypedWindowEvent(handle->x11.display, handle->x11.window, SelectionNotify, &ev) || XCheckWindowEvent(handle->x11.display, handle->x11.window, mask, &ev)) {
		XPutBackEvent(handle->x11.display, &ev);
		return 1;
	}
	return 0;
}

static void MwLLNextEventImpl(MwLL handle) {
	XEvent ev;

	if(handle->x11.clipboard_pending && (MwTimeGetTick() - handle->x11.clipboard_time) >= ClipboardTimeout) {
		Atom a = handle->x11.selection;

		handle->x11.clipboard_pending++;
		handle->x11.clipboard_time = MwTimeGetTick();

		switch(handle->x11.clipboard_pending) {
		case 2:
			a = handle->x11.compound_text;
			break;
		case 3:
			a = handle->x11.text;
			break;
		case 4:
			a = XA_STRING;
			break;
		case 5:
			handle->x11.clipboard_pending = 0;
			break;
		}

		if(a != handle->x11.selection) {
			XConvertSelection(handle->x11.display, handle->x11.clipboard, a, handle->x11.selection, handle->x11.window, CurrentTime);
		}
	}

	while(XCheckTypedWindowEvent(handle->x11.display, handle->x11.window, ClientMessage, &ev) || XCheckTypedWindowEvent(handle->x11.display, handle->x11.window, SelectionNotify, &ev) || XCheckWindowEvent(handle->x11.display, handle->x11.window, mask, &ev)) {
		int render = 0;
		if(ev.type == Expose) {
			handle->x11.force_render = 0;
			render			 = 1;
		} else if(ev.type == ButtonPress) {
			MwMouse p;
			p.point.x = ev.xbutton.x;
			p.point.y = ev.xbutton.y;
			if(ev.xbutton.button == Button1) {
				p.button = MwMOUSE_LEFT;
			} else if(ev.xbutton.button == Button2) {
				p.button = MwMOUSE_MIDDLE;
			} else if(ev.xbutton.button == Button3) {
				p.button = MwMOUSE_RIGHT;
			} else if(ev.xbutton.button == Button4) {
				p.button = MwMOUSE_WHEELUP;
			} else if(ev.xbutton.button == Button5) {
				p.button = MwMOUSE_WHEELDOWN;
			}

#ifndef ALLOW_SLOPPY_FOCUS
			XSetInputFocus(handle->x11.display, handle->x11.window, RevertToNone, CurrentTime);
#endif

			MwLLDispatch(handle, down, &p);
		} else if(ev.type == ButtonRelease) {
			MwMouse p;
			p.point.x = ev.xbutton.x;
			p.point.y = ev.xbutton.y;
			if(ev.xbutton.button == Button1) {
				p.button = MwMOUSE_LEFT;
			} else if(ev.xbutton.button == Button2) {
				p.button = MwMOUSE_MIDDLE;
			} else if(ev.xbutton.button == Button3) {
				p.button = MwMOUSE_RIGHT;
			} else if(ev.xbutton.button == Button4) {
				p.button = MwMOUSE_WHEELUP;
			} else if(ev.xbutton.button == Button5) {
				p.button = MwMOUSE_WHEELDOWN;
			}

			MwLLDispatch(handle, up, &p);
		} else if(ev.type == ConfigureNotify) {
			if(handle->x11.width != (unsigned int)ev.xconfigure.width || handle->x11.height != (unsigned int)ev.xconfigure.height) {
				MwLLDispatch(handle, resize, NULL);
				destroy_pixmap(handle);
				create_pixmap(handle);
				render = 1;
			}
			handle->x11.width  = ev.xconfigure.width;
			handle->x11.height = ev.xconfigure.height;
		} else if(ev.type == ClientMessage) {
			if(ev.xclient.message_type == handle->x11.wm_protocols && ev.xclient.data.l[0] == (long)handle->x11.wm_delete) {
				MwLLDispatch(handle, close, NULL);
			}
		} else if(ev.type == FocusIn) {
			MwLLDispatch(handle, focus_in, NULL);
		} else if(ev.type == FocusOut) {
			MwLLDispatch(handle, focus_out, NULL);
		} else if(ev.type == MotionNotify) {
			MwPoint		  p;
			XWindowAttributes attr;

			XGetWindowAttributes(handle->x11.display, handle->x11.window, &attr);

			p.x = ev.xmotion.x;
			p.y = ev.xmotion.y;

			if(handle->x11.grabbed) {
				p.x -= attr.width / 2;
				p.y -= attr.height / 2;
			}

			MwLLDispatch(handle, move, &p);
			if(handle->x11.grabbed && (p.x != 0 || p.y != 0)) {
				XWarpPointer(handle->x11.display, None, handle->x11.window, 0, 0, 0, 0, attr.width / 2, attr.height / 2);
			}
		} else if(ev.type == KeyPress || ev.type == KeyRelease) {
			int    n = -1;
			char   str[512];
			KeySym sym;

			str[0] = 0;

			XLookupString(&ev.xkey, str, 512, &sym, NULL);

			/* wtf is wrong with xlib? */
			if(strlen(str) == 0 || (str[0] < 0x20)) {
				char* s = XKeysymToString(sym);

				strcpy(str, s);
			}

			if(MwStringIsKeyUTF8(sym)) {
				char s = str[0];

				if(ev.xkey.state & (ShiftMask | LockMask) && !(ev.xkey.state & (ControlMask | Mod1Mask))) {
					n = toupper((int)s);
				} else {
					n = s;
				}
			}

			if(strcmp(str, "BackSpace") == 0) {
				n = MwKEY_BACKSPACE;
			} else if(strcmp(str, "Left") == 0) {
				n = MwKEY_LEFT;
			} else if(strcmp(str, "Right") == 0) {
				n = MwKEY_RIGHT;
			} else if(strcmp(str, "Up") == 0) {
				n = MwKEY_UP;
			} else if(strcmp(str, "Down") == 0) {
				n = MwKEY_DOWN;
			} else if(strcmp(str, "Return") == 0) {
				n = MwKEY_ENTER;
			} else if(strcmp(str, "Escape") == 0) {
				n = MwKEY_ESCAPE;
			} else if(strcmp(str, "Shift_L") == 0) {
				n = MwKEY_LEFTSHIFT;
			} else if(strcmp(str, "Shift_R") == 0) {
				n = MwKEY_RIGHTSHIFT;
			} else if(strcmp(str, "Alt_L") == 0 || strcmp(str, "Alt_R") == 0) {
				n = MwKEY_ALT;
			} else if(strcmp(str, "Control_R") == 0 || strcmp(str, "Control_R") == 0) {
				n = MwKEY_CONTROL;
			}

			if(n != MwKEY_CONTROL && ev.xkey.state & ControlMask) {
				n |= MwKEY_CONTROL_FLAG;
			}
			if(n != MwKEY_ALT && ev.xkey.state & Mod1Mask) {
				n |= MwKEY_ALT_FLAG;
			}

			if(n != -1) {
				if(ev.type == KeyPress) {
					MwLLDispatch(handle, key, &n);
				} else {
					MwLLDispatch(handle, key_released, &n);
				}
			}
		} else if(ev.type == SelectionNotify) {
			handle->x11.clipboard_pending = 0;

			if(ev.xselection.property != None) {
				Atom	       type;
				int	       format;
				unsigned long  nitems, after, size;
				unsigned char* pdat;
				int	       ret = XGetWindowProperty(handle->x11.display, handle->x11.window, handle->x11.selection, 0, 0, False, AnyPropertyType, &type, &format, &nitems, &after, &pdat);

				if(pdat != NULL) XFree(pdat);

				size = after;

				if(ret == Success && type != None && format == 8) {
					char* buf = malloc(size + 1);
					int   offset;

					for(offset = 0; after > 0; offset += nitems) {
						ret = XGetWindowProperty(handle->x11.display, handle->x11.window, handle->x11.selection, offset / 4, after / 4 + 1, False, AnyPropertyType, &type, &format, &nitems, &after, &pdat);

						if(ret != Success) {
							free(buf);
							buf = NULL;
							break;
						}

						memcpy(buf + offset, pdat, nitems);

						XFree(pdat);
					}

					if(buf != NULL) {
						buf[size] = 0;

						MwLLDispatch(handle, clipboard, buf);

						free(buf);
					}
				}

				XDeleteProperty(handle->x11.display, handle->x11.window, handle->x11.selection);
			}
		}
		if(render) {
			int	     x, y;
			unsigned int w, h;

			MwLLGetXYWH(handle, &x, &y, &w, &h);

			MwLLDispatch(handle, draw, NULL);
			if(handle->common.copy_buffer) {
				XCopyArea(handle->x11.display, handle->x11.pixmap, handle->x11.window, handle->x11.gc, 0, 0, w, h, 0, 0);
				XSetWindowBackgroundPixmap(handle->x11.display, handle->x11.window, handle->x11.pixmap);
			}
		}
	}
}

static void MwLLSetTitleImpl(MwLL handle, const char* title) {
	XSetStandardProperties(handle->x11.display, handle->x11.window, title, title, None, (char**)NULL, 0, NULL);
}

static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data, int width, int height) {
	MwLLPixmap	  r = malloc(sizeof(*r));
	int		  evbase, erbase;
	XWindowAttributes attr;

	r->common.raw = malloc(4 * width * height);
	memcpy(r->common.raw, data, 4 * width * height);

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &attr);

	r->common.width	 = width;
	r->common.height = height;

	r->x11.depth   = attr.depth;
	r->x11.display = handle->x11.display;
	r->x11.data    = malloc(sizeof(unsigned long) * width * height);
	r->x11.handle  = handle;

#ifdef USE_XRENDER
	if(xsymtbl.has_xrender) {
		r->x11.use_xrender = XRenderQueryExtension(handle->x11.display, &evbase, &erbase) ? 1 : 0;
	}
#endif

	r->x11.image = XCreateImage(handle->x11.display, DefaultVisual(handle->x11.display, DefaultScreen(handle->x11.display)), r->x11.depth, ZPixmap, 0, NULL, width, height, 32, 0);
	r->x11.mask  = XCreateImage(handle->x11.display, DefaultVisual(handle->x11.display, DefaultScreen(handle->x11.display)), 8, ZPixmap, 0, NULL, width, height, 32, 0);

	r->x11.image->data = malloc(r->x11.image->bytes_per_line * height);
	r->x11.mask->data  = malloc(r->x11.mask->bytes_per_line * height);

	MwLLPixmapUpdate(r);
	return r;
}

static void MwLLPixmapUpdateImpl(MwLLPixmap r) {
	int y, x;
	for(y = 0; y < r->common.height; y++) {
		for(x = 0; x < r->common.width; x++) {
			unsigned char* px = &r->common.raw[(y * r->common.width + x) * 4];
			MwLLColor      c  = NULL;
			unsigned long  p;

			c = MwLLAllocColor(r->x11.handle, px[0], px[1], px[2]);
			p = c->x11.pixel;
			MwLLFreeColor(c);

			XPutPixel(r->x11.image, x, y, p);
			*(unsigned long*)(&r->x11.data[(y * r->common.width + x) * sizeof(unsigned long)]) = (px[3] << 24) | p;
		}
	}

	for(y = 0; y < r->common.height; y++) {
		for(x = 0; x < r->common.width; x++) {
			if(r->common.raw[(y * r->common.width + x) * 4 + 3]) {
				XPutPixel(r->x11.mask, x, y, 255);
			} else {
				XPutPixel(r->x11.mask, x, y, 0);
			}
		}
	}
}

static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
	free(pixmap->common.raw);
	XDestroyImage(pixmap->x11.image);
	XDestroyImage(pixmap->x11.mask);
	free(pixmap->x11.data);

	free(pixmap);
}

#define DO_XRENDER(dest_px, src, format, bpp) \
	{ \
		Picture		   src_pic, dest_pic; \
		XRenderPictFormat* fmt	  = XRenderFindStandardFormat(handle->x11.display, format); \
		Pixmap		   src_px = XCreatePixmap(handle->x11.display, handle->x11.window, pixmap->common.width, pixmap->common.height, bpp); \
		GC		   gc	  = XCreateGC(handle->x11.display, src_px, 0, NULL); \
\
		dest_px = XCreatePixmap(handle->x11.display, handle->x11.window, rect->width, rect->height, bpp); \
\
		XPutImage(handle->x11.display, src_px, gc, src, 0, 0, 0, 0, pixmap->common.width, pixmap->common.height); \
\
		src_pic	 = XRenderCreatePicture(handle->x11.display, src_px, fmt, render_mask, &attr); \
		dest_pic = XRenderCreatePicture(handle->x11.display, dest_px, fmt, render_mask, &attr); \
\
		XRenderSetPictureTransform(handle->x11.display, src_pic, &m); \
		XRenderComposite(handle->x11.display, PictOpSrc, src_pic, 0, dest_pic, 0, 0, 0, 0, 0, 0, rect->width, rect->height); \
\
		XRenderFreePicture(handle->x11.display, src_pic); \
		XRenderFreePicture(handle->x11.display, dest_pic); \
		XFreePixmap(handle->x11.display, src_px); \
		XFreeGC(handle->x11.display, gc); \
	}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
	if(rect->width <= 0 || rect->height <= 0 || pixmap->common.width <= 0 || pixmap->common.height <= 0) return;
	if(rect->width >= INT16_MAX) rect->width = INT16_MAX;
	if(rect->height >= INT16_MAX) rect->height = INT16_MAX;

#ifdef USE_XRENDER
	if(pixmap->x11.image != NULL && pixmap->x11.use_xrender) {
		Pixmap			 px;
		Pixmap			 mask, mask_tmp;
		XRenderPictureAttributes attr;
		XTransform		 m;
		double			 xsc;
		double			 ysc;
		unsigned long		 render_mask = 0;
		XImage*			 mask_tmp_img;
		XImage*			 mask_img;
		int			 y, x;
		GC			 mask_gc;

		memset(&attr, 0, sizeof(attr));

		xsc = (double)pixmap->common.width / rect->width;
		ysc = (double)pixmap->common.height / rect->height;

		m.matrix[0][0] = XDoubleToFixed(xsc);
		m.matrix[0][1] = XDoubleToFixed(0);
		m.matrix[0][2] = XDoubleToFixed(0);

		m.matrix[1][0] = XDoubleToFixed(0);
		m.matrix[1][1] = XDoubleToFixed(ysc);
		m.matrix[1][2] = XDoubleToFixed(0);

		m.matrix[2][0] = XDoubleToFixed(0);
		m.matrix[2][1] = XDoubleToFixed(0);
		m.matrix[2][2] = XDoubleToFixed(1.0);

		DO_XRENDER(px, pixmap->x11.image, PictStandardRGB24, pixmap->x11.depth);
		DO_XRENDER(mask_tmp, pixmap->x11.mask, PictStandardA8, 8);

		/* FIXME: this is terrible way to do this... i hope someone who knows xrender better than me
		 * gives me better solution
		 */
		mask_tmp_img = XGetImage(handle->x11.display, mask_tmp, 0, 0, rect->width, rect->height, 0xff, ZPixmap);
		mask_img     = XCreateImage(handle->x11.display, DefaultVisual(handle->x11.display, DefaultScreen(handle->x11.display)), 1, ZPixmap, 0, NULL, rect->width, rect->height, 32, 0);
		mask	     = XCreatePixmap(handle->x11.display, handle->x11.window, rect->width, rect->height, 1);
		mask_gc	     = XCreateGC(handle->x11.display, mask, 0, NULL);

		mask_img->data = malloc(mask_img->bytes_per_line * rect->height);

		for(y = 0; y < rect->height; y++) {
			for(x = 0; x < rect->width; x++) {
				unsigned long p = XGetPixel(mask_tmp_img, x, y);
				if(p == 255) {
					p = 1;
				} else {
					p = 0;
				}
				XPutPixel(mask_img, x, y, p);
			}
		}
		XPutImage(handle->x11.display, mask, mask_gc, mask_img, 0, 0, 0, 0, rect->width, rect->height);
		XFreeGC(handle->x11.display, mask_gc);
		XDestroyImage(mask_img);
		XDestroyImage(mask_tmp_img);
		/* end terrible hack */

		XSetClipMask(handle->x11.display, handle->x11.gc, mask);
		XSetClipOrigin(handle->x11.display, handle->x11.gc, rect->x, rect->y);
		XCopyArea(handle->x11.display, px, handle->x11.pixmap, handle->x11.gc, 0, 0, rect->width, rect->height, rect->x, rect->y);
		XSetClipMask(handle->x11.display, handle->x11.gc, None);

		XFreePixmap(handle->x11.display, mask_tmp);
		XFreePixmap(handle->x11.display, mask);
		XFreePixmap(handle->x11.display, px);
	} else
#endif
	    if(pixmap->x11.image != NULL) {
		XImage* dest;
		XImage* destmask;
		Pixmap	mask   = XCreatePixmap(handle->x11.display, handle->x11.window, rect->width, rect->height, 1);
		GC	maskgc = XCreateGC(handle->x11.display, mask, 0, NULL);
		char*	di     = malloc(rect->width * rect->height * 4);
		char*	dm     = malloc(rect->width * rect->height * 4);
		int	y, x;

		dest	 = XCreateImage(handle->x11.display, DefaultVisual(handle->x11.display, DefaultScreen(handle->x11.display)), pixmap->x11.depth, ZPixmap, 0, di, rect->width, rect->height, 32, rect->width * 4);
		destmask = XCreateImage(handle->x11.display, DefaultVisual(handle->x11.display, DefaultScreen(handle->x11.display)), 1, ZPixmap, 0, dm, rect->width, rect->height, 32, rect->width * 4);

		for(y = 0; y < (int)rect->height; y++) {
			for(x = 0; x < (int)rect->width; x++) {
				int   sy = y * pixmap->common.height / rect->height;
				int   sx = x * pixmap->common.width / rect->width;
				char* ipx;
				char* opx;
				sy = (int)sy;
				sx = (int)sx;

				ipx = &pixmap->x11.image->data[(pixmap->common.width * sy + sx) * (pixmap->x11.image->bitmap_unit / 8)];
				opx = &di[(rect->width * y + x) * (pixmap->x11.image->bitmap_unit / 8)];
				memcpy(opx, ipx, pixmap->x11.image->bitmap_unit / 8);

				XPutPixel(destmask, x, y, XGetPixel(pixmap->x11.mask, sx, sy));
			}
		}

		XPutImage(handle->x11.display, mask, maskgc, destmask, 0, 0, 0, 0, rect->width, rect->height);

		XSetClipMask(handle->x11.display, handle->x11.gc, mask);
		XSetClipOrigin(handle->x11.display, handle->x11.gc, rect->x, rect->y);
		XPutImage(handle->x11.display, handle->x11.pixmap, handle->x11.gc, dest, 0, 0, rect->x, rect->y, rect->width, rect->height);
		XSetClipMask(handle->x11.display, handle->x11.gc, None);

		XDestroyImage(dest);
		XDestroyImage(destmask);

		XFreeGC(handle->x11.display, maskgc);
		XFreePixmap(handle->x11.display, mask);
	}
}

static void MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {
	unsigned long* icon = malloc((2 + pixmap->common.width * pixmap->common.height) * sizeof(*icon));
	int	       i;
	Atom	       atom = XInternAtom(handle->x11.display, "_NET_WM_ICON", False);

	icon[0] = pixmap->common.width;
	icon[1] = pixmap->common.height;

	for(i = 0; i < pixmap->common.width * pixmap->common.height; i++) {
		icon[2 + i] = *(unsigned long*)(&pixmap->x11.data[i * sizeof(unsigned long)]);
	}

	XChangeProperty(handle->x11.display, handle->x11.window, atom, 6, 32, PropModeReplace, (unsigned char*)icon, 2 + pixmap->common.width * pixmap->common.height);

	free(icon);
}

static void MwLLForceRenderImpl(MwLL handle) {
	if(!handle->x11.force_render) {
		XEvent ev;
		memset(&ev, 0, sizeof(ev));

		ev.type		  = Expose;
		ev.xexpose.window = handle->x11.window;
		XSendEvent(handle->x11.display, handle->x11.window, False, ExposureMask, &ev);

		handle->x11.force_render = 1;
	}
}

Cursor MwLLX11CreateCursor(Display* display, MwCursor* image, MwCursor* mask) {
	Cursor	cur;
	int	y, x, ys, xs;
	char*	di	= malloc(MwCursorDataHeight * MwCursorDataHeight * 4);
	char*	dm	= malloc(MwCursorDataHeight * MwCursorDataHeight * 4);
	XImage* cimage	= XCreateImage(display, DefaultVisual(display, DefaultScreen(display)), 1, ZPixmap, 0, di, MwCursorDataHeight, MwCursorDataHeight, 32, MwCursorDataHeight * 4);
	XImage* cmask	= XCreateImage(display, DefaultVisual(display, DefaultScreen(display)), 1, ZPixmap, 0, dm, MwCursorDataHeight, MwCursorDataHeight, 32, MwCursorDataHeight * 4);
	Pixmap	pimage	= XCreatePixmap(display, DefaultRootWindow(display), MwCursorDataHeight, MwCursorDataHeight, 1);
	Pixmap	pmask	= XCreatePixmap(display, DefaultRootWindow(display), MwCursorDataHeight, MwCursorDataHeight, 1);
	GC	imagegc = XCreateGC(display, pimage, 0, NULL);
	GC	maskgc	= XCreateGC(display, pmask, 0, NULL);
	XColor	cfg, cbg;

	xs = -mask->x + image->x;
	ys = mask->height + mask->y;
	ys = image->height + image->y - ys;

	if(ys < 0) ys = -ys;

	memset(cimage->data, 0, cimage->bytes_per_line * cimage->height);
	memset(cmask->data, 0, cmask->bytes_per_line * cmask->height);
	for(y = 0; y < mask->height; y++) {
		unsigned int l = mask->data[y];
		for(x = mask->width - 1; x >= 0; x--) {
			if(l & 1) {
				XPutPixel(cmask, x, y, 1);
			}
			l = l >> 1;
		}
	}
	for(y = 0; y < image->height; y++) {
		unsigned int l = image->data[y];
		for(x = image->width - 1; x >= 0; x--) {
			int px = 0;
			if(l & 1) px = 1;
			XPutPixel(cimage, xs + x, ys + y, px);

			l = l >> 1;
		}
	}

	cfg.red	  = 65535;
	cfg.green = 65535;
	cfg.blue  = 65535;
	XAllocColor(display, DefaultColormap(display, DefaultScreen(display)), &cfg);

	cbg.red	  = 0;
	cbg.green = 0;
	cbg.blue  = 0;
	XAllocColor(display, DefaultColormap(display, DefaultScreen(display)), &cbg);

	XPutImage(display, pimage, imagegc, cimage, 0, 0, 0, 0, MwCursorDataHeight, MwCursorDataHeight);
	XPutImage(display, pmask, maskgc, cmask, 0, 0, 0, 0, MwCursorDataHeight, MwCursorDataHeight);

	cur = XCreatePixmapCursor(display, pimage, pmask, &cfg, &cbg, xs - image->x, ys + (MwCursorDataHeight + image->y));

	XFreePixmap(display, pimage);
	XFreePixmap(display, pmask);

	XDestroyImage(cimage);
	XDestroyImage(cmask);

	XFreeGC(display, imagegc);
	XFreeGC(display, maskgc);

	return cur;
}

static void MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {
	Cursor cur = MwLLX11CreateCursor(handle->x11.display, image, mask);

	XDefineCursor(handle->x11.display, handle->x11.window, cur);
	XFreeCursor(handle->x11.display, cur);
}

static void MwLLDetachImpl(MwLL handle, MwPoint* point) {
	int		  x = 0, y = 0;
	Window		  child, root, parent;
	Window*		  children;
	unsigned int	  nchild;
	XWindowAttributes xwa;

	handle->x11.top = 1;

	XQueryTree(handle->x11.display, handle->x11.window, &root, &parent, &children, &nchild);
	if(children != NULL) XFree(children);

	XTranslateCoordinates(handle->x11.display, parent, RootWindow(handle->x11.display, DefaultScreen(handle->x11.display)), 0, 0, &x, &y, &child);

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &xwa);

	XReparentWindow(handle->x11.display, handle->x11.window, RootWindow(handle->x11.display, DefaultScreen(handle->x11.display)), x + point->x, y + point->y);

	if(xwa.map_state == IsViewable) {
		sync_move(handle, x + point->x, y + point->y);
	}
}

static void MwLLShowImpl(MwLL handle, int show) {
	if(show) {
		wait_map(handle, 1);

#ifndef ALLOW_SLOPPY_FOCUS
		XSetInputFocus(handle->x11.display, handle->x11.window, RevertToNone, CurrentTime);
#endif
	} else {
		wait_unmap(handle, 1);
	}
}

static void MwLLMakePopupImpl(MwLL handle, MwLL parent) {
	Atom wndtype  = XInternAtom(handle->x11.display, "_NET_WM_WINDOW_TYPE", False);
	Atom wnddlg   = XInternAtom(handle->x11.display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	Atom wndstate = XInternAtom(handle->x11.display, "_NET_WM_STATE", False);
	Atom wndmodal = XInternAtom(handle->x11.display, "_NET_WM_STATE_MODAL", False);

	if(parent != NULL) XSetTransientForHint(handle->x11.display, handle->x11.window, parent->x11.window);
	XChangeProperty(handle->x11.display, handle->x11.window, wndtype, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wnddlg, 1);
	XChangeProperty(handle->x11.display, handle->x11.window, wndstate, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wndmodal, 1);
}

static void MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx, int maxy) {
	XSizeHints* hints = XAllocSizeHints();
	long	    ret;

	XGetWMSizeHints(handle->x11.display, handle->x11.window, hints, &ret, XA_WM_NORMAL_HINTS);

	hints->flags |= PMinSize | PMaxSize;
	hints->min_width  = minx;
	hints->min_height = miny;
	hints->max_width  = maxx;
	hints->max_height = maxy;
	XSetWMSizeHints(handle->x11.display, handle->x11.window, hints, XA_WM_NORMAL_HINTS);
	XFree(hints);
}

static void MwLLMakeBorderlessImpl(MwLL handle, int toggle) {
	Atom	    atom = XInternAtom(handle->x11.display, "_MOTIF_WM_HINTS", 0);
	mwm_hints_t hints;

	hints.flags	  = MWM_HINTS_DECORATIONS;
	hints.decorations = toggle ? 0 : 1;
	XChangeProperty(handle->x11.display, handle->x11.window, atom, atom, 32, PropModeReplace, (unsigned char*)&hints, 5);
}

static void MwLLFocusImpl(MwLL handle) {
	XSetInputFocus(handle->x11.display, handle->x11.window, RevertToNone, CurrentTime);
}

static void MwLLGrabPointerImpl(MwLL handle, int toggle) {
	XWindowAttributes attr;

	XGetWindowAttributes(handle->x11.display, handle->x11.window, &attr);

	if(toggle) {
		handle->x11.grabbed = 1;

		XWarpPointer(handle->x11.display, None, handle->x11.window, 0, 0, 0, 0, attr.width / 2, attr.height / 2);
	} else {
		handle->x11.grabbed = 0;
	}
}

static void MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {
	/* TODO */

	(void)handle;
	(void)text;
	(void)clipboard_type;
}

static void MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {
	(void)clipboard_type;

	if(handle->x11.clipboard_pending) return;

	XConvertSelection(handle->x11.display, handle->x11.clipboard, handle->x11.utf8_string, handle->x11.selection, handle->x11.window, CurrentTime);

	handle->x11.clipboard_pending = 1;
	handle->x11.clipboard_time    = MwTimeGetTick();
}

static void MwLLMakeToolWindowImpl(MwLL handle) {
	XSetWindowAttributes xswa;
	Atom		     wndtype = XInternAtom(handle->x11.display, "_NET_WM_WINDOW_TYPE", False);
	Atom		     wndmenu = XInternAtom(handle->x11.display, "_NET_WM_WINDOW_TYPE_MENU", False);

	xswa.override_redirect = True;

	XChangeWindowAttributes(handle->x11.display, handle->x11.window, CWOverrideRedirect, &xswa);
	XChangeProperty(handle->x11.display, handle->x11.window, wndtype, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wndmenu, 1);
}

static void MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {
	Window	     root, child;
	int	     rx, ry, wx, wy;
	unsigned int m;

	XQueryPointer(handle->x11.display, DefaultRootWindow(handle->x11.display), &root, &child, &rx, &ry, &wx, &wy, &m);

	point->x = rx;
	point->y = ry;
}

static void MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {
	XWindowAttributes xwa;
	XGetWindowAttributes(handle->x11.display, DefaultRootWindow(handle->x11.display), &xwa);

	rect->x = rect->y = 0;
	rect->width	  = xwa.width;
	rect->height	  = xwa.height;
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
	return MwTRUE;
}

static void MwLLRaiseImpl(MwLL handle) {
	XRaiseWindow(handle->x11.display, handle->x11.window);
}

static void MwLLClipImpl(MwLL handle, MwRect* rect) {
	if(rect == NULL) {
		XSetClipMask(handle->x11.display, handle->x11.gc, None);
	} else {
		XRectangle rc;

		rc.x	  = 0;
		rc.y	  = 0;
		rc.width  = rect->width;
		rc.height = rect->height;

		XSetClipRectangles(handle->x11.display, handle->x11.gc, rect->x, rect->y, &rc, 1, Unsorted);
	}
}

static int MwLLX11CallInitImpl(void) {
	MwBool loadX11 = MwFALSE;
	if(getenv("MILSKO_BACKEND")) {
		loadX11 |=
		    (strcmp(getenv("MILSKO_BACKEND"), "x11") == 0);
	} else if(getenv("DISPLAY")) {
		loadX11 |= (getenv("DISPLAY") != NULL);
	}

	if(!loadX11) {
		return 1;
	}

#ifdef __APPLE__
	xsymtbl.lib_xlib = MwDynamicOpen("libX11.dylib");
#else
	xsymtbl.lib_xlib = MwDynamicOpen("libX11.so");
#endif
	if(!xsymtbl.lib_xlib) {
		return 1;
	}

	xsymtbl.has_xrender = MwFALSE;
#if USE_XRENDER
	xsymtbl.lib_xrender = MwDynamicOpen("libXrender.so");
	if(xsymtbl.lib_xrender) {
		xsymtbl.has_xrender = MwTRUE;
	}
#endif

#define X11_FUNC_LOAD(x) x = MwDynamicSymbol(xsymtbl.lib_xlib, #x)

	X11_FUNC_LOAD(XClearWindow);
	X11_FUNC_LOAD(XCreateImage);
	X11_FUNC_LOAD(XOpenDisplay);
	X11_FUNC_LOAD(XKeysymToString);
	X11_FUNC_LOAD(XInternAtom);
	X11_FUNC_LOAD(XCreatePixmapCursor);
	X11_FUNC_LOAD(XCreateGC);
	X11_FUNC_LOAD(XCreatePixmap);
	X11_FUNC_LOAD(XCreateSimpleWindow);
	X11_FUNC_LOAD(XVisualIDFromVisual);
	X11_FUNC_LOAD(XRootWindow);
	X11_FUNC_LOAD(XSetWMProtocols);
	X11_FUNC_LOAD(XSetTransientForHint);
	X11_FUNC_LOAD(XAllocColor);
	X11_FUNC_LOAD(XChangeProperty);
	X11_FUNC_LOAD(XChangeWindowAttributes);
	X11_FUNC_LOAD(XCheckTypedWindowEvent);
	X11_FUNC_LOAD(XCheckWindowEvent);
	X11_FUNC_LOAD(XCloseDisplay);
	X11_FUNC_LOAD(XConfigureWindow);
	X11_FUNC_LOAD(XConvertSelection);
	X11_FUNC_LOAD(XCopyArea);
	X11_FUNC_LOAD(XDefaultScreen);
	X11_FUNC_LOAD(XDefineCursor);
	X11_FUNC_LOAD(XDeleteProperty);
	X11_FUNC_LOAD(XDestroyWindow);
	X11_FUNC_LOAD(XDrawLine);
	X11_FUNC_LOAD(XFillPolygon);
	X11_FUNC_LOAD(XFree);
	X11_FUNC_LOAD(XFreeCursor);
	X11_FUNC_LOAD(XFreeGC);
	X11_FUNC_LOAD(XFreePixmap);
	X11_FUNC_LOAD(XGetGeometry);
	X11_FUNC_LOAD(XGetWindowProperty);
	X11_FUNC_LOAD(XGetWindowAttributes);
	X11_FUNC_LOAD(XMapWindow);
	X11_FUNC_LOAD(XNextEvent);
	X11_FUNC_LOAD(XPending);
	X11_FUNC_LOAD(XPutBackEvent);
	X11_FUNC_LOAD(XPutImage);
	X11_FUNC_LOAD(XQueryPointer);
	X11_FUNC_LOAD(XQueryTree);
	X11_FUNC_LOAD(XReparentWindow);
	X11_FUNC_LOAD(XSelectInput);
	X11_FUNC_LOAD(XSendEvent);
	X11_FUNC_LOAD(XSetClipMask);
	X11_FUNC_LOAD(XSetClipOrigin);
	X11_FUNC_LOAD(XSetClipRectangles);
	X11_FUNC_LOAD(XSetForeground);
	X11_FUNC_LOAD(XSetGraphicsExposures);
	X11_FUNC_LOAD(XSetInputFocus);
	X11_FUNC_LOAD(XSetWindowBackgroundPixmap);
	X11_FUNC_LOAD(XSync);
	X11_FUNC_LOAD(XTranslateCoordinates);
	X11_FUNC_LOAD(XUnmapWindow);
	X11_FUNC_LOAD(XWarpPointer);
	X11_FUNC_LOAD(XSetLocaleModifiers);
	X11_FUNC_LOAD(XOpenIM);
	X11_FUNC_LOAD(XCloseIM);
	X11_FUNC_LOAD(XCreateIC);
	X11_FUNC_LOAD(XDestroyIC);
	X11_FUNC_LOAD(XSetICFocus);
	X11_FUNC_LOAD(XGetImage);
	X11_FUNC_LOAD(XRaiseWindow);

	X11_FUNC_LOAD(XAllocSizeHints);
	X11_FUNC_LOAD(XGetVisualInfo);
	X11_FUNC_LOAD(XGetWMNormalHints);
	X11_FUNC_LOAD(XGetWMSizeHints);
	X11_FUNC_LOAD(XLookupString);
	X11_FUNC_LOAD(XSetStandardProperties);
	X11_FUNC_LOAD(XSetWMNormalHints);
	X11_FUNC_LOAD(XSetWMSizeHints);

#if USE_XRENDER
#define XRENDER_FUNC_LOAD(x) x = MwDynamicSymbol(xsymtbl.lib_xrender, #x);

	XRENDER_FUNC_LOAD(XRenderQueryExtension)
	XRENDER_FUNC_LOAD(XRenderFindStandardFormat)
	XRENDER_FUNC_LOAD(XRenderCreatePicture)
	XRENDER_FUNC_LOAD(XRenderFreePicture)
	XRENDER_FUNC_LOAD(XRenderSetPictureTransform)
	XRENDER_FUNC_LOAD(XRenderComposite)
#endif

#ifdef USE_DBUS
	xsymtbl.has_dbus = MwLLDBusFuncSetup(&xsymtbl.dbus);
#endif
	return 0;
}

#include "call.c"
CALL(X11);
