/*!
 * @file Mw/LowLevel/X11.h
 * @brief X11 Backend
 * @warning This is used internally
 */
#ifndef __MW_LOWLEVEL_X11_H__
#define __MW_LOWLEVEL_X11_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/LowLevel.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef USE_XRENDER
#include <X11/extensions/Xrender.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct _MwLLX11 {
	struct _MwLLCommon common;

	unsigned int width;
	unsigned int height;

	Display* display;
	Window	 window;
	Pixmap	 pixmap;
	GC	 gc;
	Colormap colormap;
	Atom	 wm_delete;
	Atom	 wm_protocols;
	Atom	 utf8_string;
	Atom	 compound_text;
	Atom	 text;
	Atom	 clipboard;
	Atom	 selection;
	XIM	 xim;
	XIC	 xic;

	long clipboard_time;
	int  clipboard_pending; /* 1 if UTF8_STRING, 2 if COMPOUND_TEXT, 3 if TEXT, 4 if STRING, otherwise 0 */

	int top;
	int toplevel;
	int grabbed;
	int force_render;

	unsigned long red_mask;
	unsigned long red_max;
	unsigned long red_shift;

	unsigned long green_mask;
	unsigned long green_max;
	unsigned long green_shift;

	unsigned long blue_mask;
	unsigned long blue_max;
	unsigned long blue_shift;

#ifdef USE_DBUS
	MwLLDBusContext dbus;
#endif

	MwBool dark_theme_detection;
};

struct _MwLLX11Color {
	struct _MwLLCommonColor common;

	unsigned long pixel;
};

struct _MwLLX11Pixmap {
	struct _MwLLCommonPixmap common;

	int	       depth;
	unsigned char* data;
	MwLL	       handle;
	int	       use_xrender;
	Display*       display;
	XImage*	       image;
	XImage*	       mask;
};

MWDECL int    MwLLX11CallInit(void);
MWDECL Cursor MwLLX11CreateCursor(Display* display, MwCursor* image, MwCursor* mask);

#ifdef __cplusplus
}
#endif

#endif
