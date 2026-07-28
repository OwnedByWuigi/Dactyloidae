/*!
 * @file Mw/LowLevel.h
 * @brief Low-level drawing API
 * @warning This is used internally
 */
#ifndef __MW_LOWLEVEL_H__
#define __MW_LOWLEVEL_H__

#include <Mw/BaseTypes.h>
#include <Mw/MachDep.h>

typedef struct _MwLLHandler*	  MwLLHandler;
typedef struct _MwLLCommon*	  MwLLCommon;
typedef struct _MwLLCommonColor*  MwLLCommonColor;
typedef struct _MwLLCommonPixmap* MwLLCommonPixmap;
#ifdef _MILSKO
typedef union _MwLL*	   MwLL;
typedef union _MwLLColor*  MwLLColor;
typedef union _MwLLPixmap* MwLLPixmap;
typedef struct _MwFLFont*  MwFLFont;
#else
typedef void* MwLL;
typedef void* MwLLColor;
typedef void* MwLLPixmap;
typedef void* MwFLFont;
#endif

enum MwLLBackends {
	MwLLBackendX11 = 0,
	MwLLBackendGDI,
	MwLLBackendWayland,
	MwLLBackendCocoa,
	MwLLBackendHaiku
};

struct _MwLLCommon {
	void*  user;
	int    copy_buffer;
	int    type;
	int    coordinate_type;
	MwBool supports_transparency;

	MwLLHandler handler;
};

struct _MwLLCommonColor {
	MwU8 red;
	MwU8 green;
	MwU8 blue;
};

struct _MwLLCommonPixmap {
	int	       width;
	int	       height;
	unsigned char* raw;
	void*	       user;
};

#ifdef USE_DBUS
typedef struct _MwLLDBusFuncTable {
	void* lib;

	void (*dbus_error_init)(DBusError* error);
	DBusConnection* (*dbus_bus_get)(DBusBusType type, DBusError* error);
	dbus_bool_t (*dbus_error_is_set)(const DBusError* error);
	void (*dbus_error_free)(DBusError* error);
	DBusMessage* (*dbus_message_new_method_call)(const char* bus_name, const char* path, const char* iface, const char* method);
	void (*dbus_message_iter_init_append)(DBusMessage* message, DBusMessageIter* iter);
	dbus_bool_t (*dbus_message_iter_append_basic)(DBusMessageIter* iter, int type, const void* value);
	void (*dbus_connection_flush)(DBusConnection* connection);
	dbus_bool_t (*dbus_connection_read_write)(DBusConnection* connection, int timeout_milliseconds);
	DBusMessage* (*dbus_connection_send_with_reply_and_block)(DBusConnection* connection, DBusMessage* message, int timeout_milliseconds, DBusError* error);
	DBusMessage* (*dbus_connection_pop_message)(DBusConnection* connection);
	void (*dbus_bus_add_match)(DBusConnection* connection, const char* rule, DBusError* error);
	void (*dbus_message_unref)(DBusMessage* message);
	dbus_bool_t (*dbus_message_iter_init)(DBusMessage* message, DBusMessageIter* iter);
	dbus_bool_t (*dbus_message_is_signal)(DBusMessage* message, const char* iface, const char* signal_name);
	int (*dbus_message_iter_get_arg_type)(DBusMessageIter* iter);
	void (*dbus_message_iter_recurse)(DBusMessageIter* iter, DBusMessageIter* sub);
	void (*dbus_message_iter_next)(DBusMessageIter* iter);
	void (*dbus_connection_unref)(DBusConnection* connection);
	void (*dbus_message_iter_get_basic)(DBusMessageIter* iter, void* value);
} MwLLDBusFuncTable;

typedef struct _MwLLDBusContext {
	DBusConnection* dbus_conn;
	DBusError	dbus_err;
	DBusMessage*	dbus_msg;
	DBusMessage*	dbus_reply;
} MwLLDBusContext;

typedef void (*MwLLDBusPortalPollListener)(MwLL handle, MwU32 new_value);

/* setup the dbus func table. Returns false and prints a warning if dbus couldn't be found.  */
MWDECL MwBool MwLLDBusFuncSetup(MwLLDBusFuncTable* tbl);

MWDECL MwBool MwLLDBusNewContext(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx);
MWDECL MwBool MwLLDBusPortalGet(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx, const char* portal, const char* namespace, const char* key, void* out);
MWDECL MwBool MwLLDBusPortalWatch(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx, const char* portal);
MWDECL MwBool MwLLDBusPortalPoll(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx, MwLL handle, const char* portal, const char* namespace, const char* key, MwLLDBusPortalPollListener);
MWDECL void   MwLLDBusFreeContext(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx);

#endif

#ifdef _MILSKO
#ifdef USE_X11
#include <Mw/LowLevel/X11.h>
#endif
#ifdef USE_GDI
#include <Mw/LowLevel/GDI.h>
#endif
#ifdef USE_WAYLAND
#include <Mw/LowLevel/Wayland.h>
#endif
#ifdef USE_COCOA
#include <Mw/LowLevel/Cocoa.h>
#endif
#ifdef CLASSIC_MAC_OS
#include <Mw/LowLevel/ClassicMacOS.h>
#endif
#ifdef USE_HAIKU
#include <Mw/LowLevel/Haiku.h>
#endif

union _MwLL {
	struct _MwLLCommon common;
#ifdef USE_X11
	struct _MwLLX11 x11;
#endif
#ifdef USE_GDI
	struct _MwLLGDI gdi;
#endif
#ifdef USE_WAYLAND
	struct _MwLLWayland wayland;
	struct _MwLLCairo   cairo;
#endif
#ifdef USE_COCOA
	struct _MwLLCocoa cocoa;
#endif
#ifdef CLASSIC_MAC_OS
	struct _MwLLClassicMacOS cmacos;
#endif
#ifdef USE_HAIKU
	struct _MwLLHaiku haiku;
#endif
};

union _MwLLColor {
	struct _MwLLCommonColor common;
#ifdef USE_X11
	struct _MwLLX11Color x11;
#endif
#ifdef USE_GDI
	struct _MwLLGDIColor gdi;
#endif
#ifdef USE_WAYLAND
	struct _MwLLCairoColor wayland;
#endif
#ifdef USE_COCOA
	struct _MwLLCocoaColor cocoa;
#endif
#ifdef USE_HAIKU
	struct _MwLLHaikuColor haiku;
#endif
};

union _MwLLPixmap {
	struct _MwLLCommonPixmap common;
#ifdef USE_X11
	struct _MwLLX11Pixmap x11;
#endif
#ifdef USE_GDI
	struct _MwLLGDIPixmap gdi;
#endif
#ifdef USE_WAYLAND
	struct _MwLLWaylandPixmap wayland;
	struct _MwLLCairoPixmap	  cairo; /* same structure but we do this for code clarity */
#endif
#ifdef USE_COCOA
	struct _MwLLCocoaPixmap cocoa;
#endif
#ifdef CLASSIC_MAC_OS
	struct _MwLLClassicMacOSPixmap cmacos;
#endif
#ifdef USE_HAIKU
	struct _MwLLHaikuPixmap haiku;
#endif
};
#endif
#include <Mw/TypeDefs.h>

#define MwLLDispatch(x, y, z) \
	if(x->common.handler != NULL && x->common.handler->y != NULL) x->common.handler->y(x, z)

struct _MwLLHandler {
	void (*draw)(MwLL handle, void* data);
	void (*up)(MwLL handle, void* data);
	void (*down)(MwLL handle, void* data);
	void (*resize)(MwLL handle, void* data);
	void (*close)(MwLL handle, void* data);
	void (*move)(MwLL handle, void* data);
	void (*key)(MwLL handle, void* data);
	void (*key_released)(MwLL handle, void* data);
	void (*focus_in)(MwLL handle, void* data);
	void (*focus_out)(MwLL handle, void* data);
	void (*clipboard)(MwLL handle, void* data);
	void (*dark_theme)(MwLL handle, void* data);
};

struct _MwLLTextDispatchTable {
	int (*drawText)(MwWidget handle, MwPoint* point, const char* text, int bold, int align, MwLLColor color);
	int (*textWidth)(MwWidget handle, const char* text);
	int (*textHeight)(MwWidget handle, int count);
	void* (*fontLoad)(unsigned char* data, unsigned int size);
	void (*fontFree)(void* handle);
};

#define MwFLFlagMonospace (1 << 0)
#define MwFLFlagBold (1 << 1)
#define MwFLFlagBitmap (1 << 2)
#define MwFLBuildFont(x) ((MwFLFont)(void*)(size_t)(x))

#ifdef __cplusplus
extern "C" {
#endif

/* lowlevel.c, common part */
MWDECL void MwLLCreateCommon(MwLL handle);
MWDECL void MwLLDestroyCommon(MwLL handle);

/* driver-specific, these get assigned by backend */
MWDECL MwLL (*MwLLCreate)(MwLL parent, int x, int y, int width, int height);
MWDECL void (*MwLLDestroy)(MwLL handle);

MWDECL void (*MwLLPolygon)(MwLL handle, MwPoint* points, int points_count, MwLLColor color);
MWDECL void (*MwLLLine)(MwLL handle, MwPoint* points, MwLLColor color);

/*!
 * @brief Begin drawing sequence
 * @param handle Handle
 */
MWDECL void (*MwLLBeginDraw)(MwLL handle);

/*!
 * @brief End drawing sequence
 * @param handle Handle
 */
MWDECL void (*MwLLEndDraw)(MwLL handle);

MWDECL MwLLColor (*MwLLAllocColor)(MwLL handle, int r, int g, int b);
MWDECL void (*MwLLColorUpdate)(MwLL handle, MwLLColor c, int r, int g, int b);
MWDECL void (*MwLLFreeColor)(MwLLColor color);

MWDECL void (*MwLLGetXYWH)(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h);
MWDECL void (*MwLLSetXY)(MwLL handle, int x, int y);
MWDECL void (*MwLLSetWH)(MwLL handle, int w, int h);

MWDECL void (*MwLLSetTitle)(MwLL handle, const char* title);

MWDECL int (*MwLLPending)(MwLL handle);
MWDECL void (*MwLLNextEvent)(MwLL handle);

MWDECL MwLLPixmap (*MwLLCreatePixmap)(MwLL handle, unsigned char* data, int width, int height);
MWDECL void (*MwLLPixmapUpdate)(MwLLPixmap pixmap);
MWDECL void (*MwLLDestroyPixmap)(MwLLPixmap pixmap);
MWDECL void (*MwLLDrawPixmap)(MwLL handle, MwRect* rect, MwLLPixmap pixmap);
MWDECL void (*MwLLSetIcon)(MwLL handle, MwLLPixmap pixmap);

MWDECL void (*MwLLForceRender)(MwLL handle);

MWDECL void (*MwLLSetCursor)(MwLL handle, MwCursor* image, MwCursor* mask);

/*!
 * @brief Detach handle and make it toplevel window
 * @param handle Handle
 * @param point Point to be detached at, relative from the parent handle
 */
MWDECL void (*MwLLDetach)(MwLL handle, MwPoint* point);
MWDECL void (*MwLLShow)(MwLL handle, int show);

MWDECL void (*MwLLSetSizeHints)(MwLL handle, int minx, int miny, int maxx, int maxy);
MWDECL void (*MwLLMakeBorderless)(MwLL handle, int toggle);

/*!
 * @brief Make handle "tool" window, e.g. Submenu, combobox menu, and etc.
 * @param handle Handle
 */
MWDECL void (*MwLLMakeToolWindow)(MwLL handle);
MWDECL void (*MwLLMakePopup)(MwLL handle, MwLL parent);

/*!
 * @brief Begin changing state (has to be called before `MwLLSetSizeHints`, `MwLLMakeBorderless`, `MwLLMakeToolWindow`, and `MwLLMakePopup`)
 * @param handle Handle
 */
MWDECL void (*MwLLBeginStateChange)(MwLL handle);

/*!
 * @brief End changing state
 * @param handle Handle
 */
MWDECL void (*MwLLEndStateChange)(MwLL handle);

MWDECL void (*MwLLFocus)(MwLL handle);

/*!
 * @brief Grab pointer, and keep moving pointer to center of handle
 * @param handle Handle
 * @param toggle Toggle
 */
MWDECL void (*MwLLGrabPointer)(MwLL handle, int toggle);

MWDECL void (*MwLLSetClipboard)(MwLL handle, const char* text, int clipboard_type);
MWDECL void (*MwLLGetClipboard)(MwLL handle, int clipboard_type);

MWDECL void (*MwLLGetCursorCoord)(MwLL handle, MwPoint* point);
MWDECL void (*MwLLGetScreenSize)(MwLL handle, MwRect* rect);

/*!
 * @brief Set any extra parameters for dark theme
 * @param handle Handle
 * @param toggle Toggle
 * @warning Do not run `handle->common.handler->dark_theme` when this function gets called
 * @warning This is for stuff like, i.e. setting the accent color on modern Windows. The user themselves sets dark theme with MwNdarkTheme.
 */
MWDECL void (*MwLLSetDarkTheme)(MwLL handle, int toggle);

/*!
 * @brief Whether or not the platform should default to modern theme.
 * @param handle Handle
 * @return whether to default.
 */
MWDECL MwBool (*MwLLDoModern)(MwLL handle);

MWDECL void (*MwLLRaise)(MwLL handle);

MWDECL void (*MwLLClip)(MwLL handle, MwRect* rect);

/* font renderer */
MWDECL void MwFLSetup(void);

#ifdef USE_FREETYPE2
MWDECL int MwFL_FT2Setup(void);
#endif
#ifdef USE_STB_TRUETYPE
MWDECL int MwFL_STBTTSetup(void);
#endif

#ifdef USE_GDI_TEXT
MWDECL int MwFL_GDISetup(void);
#endif

MWDECL int (*MwFLDrawText)(MwWidget handle, MwFLFont ttf, MwPoint* point, const char* text, MwLLColor color);
MWDECL int (*MwFLTextWidth)(MwFLFont ttf, const char* text);
MWDECL int (*MwFLTextHeight)(MwFLFont ttf, int count);
MWDECL int (*MwFLTextHeightWithText)(MwFLFont ttf, const char* text);
MWDECL void* (*MwFLFontLoad)(unsigned char* data, unsigned int size, int px);
MWDECL void (*MwFLFontFree)(void* handle);

#ifdef __cplusplus
}
#endif

#endif
