/*!
 * @file Mw/TypeDefs.h
 * @brief Type definitions
 */
#ifndef __MW_TYPEDEFS_H__
#define __MW_TYPEDEFS_H__

#include <Mw/MachDep.h>
#include <Mw/BaseTypes.h>

typedef struct _MwClass *	      MwClass, MwClassRec;
typedef struct _MwIntegerKeyValue     MwIntegerKeyValue;
typedef struct _MwTextKeyValue	      MwTextKeyValue;
typedef struct _MwUserHandlerArray    MwUserHandlerArray;
typedef struct _MwUserHandlerKeyValue MwUserHandlerKeyValue;
typedef struct _MwVoidKeyValue	      MwVoidKeyValue;
typedef struct _MwMenu*		      MwMenu;
typedef struct _MwEntry*	      MwEntry;
typedef struct _MwViewport*	      MwViewport;
typedef struct _MwListBox*	      MwListBox;
typedef struct _MwComboBox*	      MwComboBox;
typedef struct _MwTreeView*	      MwTreeView;
typedef struct _MwScrollBar*	      MwScrollBar;
typedef struct _MwLabel*	      MwLabel;
typedef struct _MwLabelSegment	      MwLabelSegment;
typedef struct _MwListBoxEntry	      MwListBoxEntry;
typedef struct _MwTreeViewEntry	      MwTreeViewEntry;
typedef struct _MwDirectoryEntry      MwDirectoryEntry;
typedef struct _MwBox*		      MwBox;
typedef struct _MwSubWindow*	      MwSubWindow;
typedef struct _MwTab*		      MwTab;
typedef struct _MwTable*	      MwTable;
typedef struct _MwMouse		      MwMouse;
typedef struct _MwTTFInfo*	      MwTTFInfo;
#ifdef _MILSKO
typedef struct _MwWidget* MwWidget;
#else
typedef void* MwWidget;
#endif
typedef void (*MwHandler)(MwWidget handle);
typedef int (*MwHandlerWithStatus)(MwWidget handle);
typedef void (*MwHandlerProps)(MwWidget handle, char** key);
typedef void (*MwHandlerProp)(MwWidget handle, const char* key);
typedef void (*MwHandlerChildrenUpdate)(MwWidget handle, MwWidget child, int new_child);
typedef void (*MwHandlerChildrenProp)(MwWidget handle, MwWidget child, const char* key);
typedef void (*MwHandlerKey)(MwWidget handle, int key);
typedef void (*MwHandlerMouse)(MwWidget handle, void* ptr);
typedef void (*MwHandlerExecute)(MwWidget handle, const char* name, void* out, va_list args);
typedef void (*MwHandlerClipboardReceived)(MwWidget handle, const char* data);

typedef void(MWAPI* MwUserHandler)(MwWidget handle, void* user_data, void* call_data);
typedef void(MWAPI* MwErrorHandler)(int code, const char* message, void* user_data);

struct _MwTextKeyValue {
	char* key;
	char* value;
};

struct _MwIntegerKeyValue {
	char* key;
	int   value;
};

struct _MwUserHandlerArray {
	void*	      user_data;
	MwUserHandler handler;
};

struct _MwUserHandlerKeyValue {
	char*		    key;
	MwUserHandlerArray* value;
};

struct _MwVoidKeyValue {
	char* key;
	void* value;
};

#include <Mw/LowLevel.h>

#ifdef _MILSKO
struct _MwWidget {
	char*	  name;
	MwLLColor bgcolor;

	MwLL	  lowlevel;
	MwWidget  parent;
	MwWidget* children;
	MwClass	  widget_class;

	int	pressed;
	MwPoint mouse_point;
	int	close;
	int	prop_event;

	void* internal;
	void* opaque;
	void* user;
	void (*draw_inject)(MwWidget handle);
	void (*destroy_inject)(MwWidget handle);

	MwIntegerKeyValue*     integer;
	MwTextKeyValue*	       text;
	MwUserHandlerKeyValue* handler;
	MwVoidKeyValue*	       data;

	MwWidget* destroy_queue;
	MwWidget* tick_list;

	int destroyed;

	void* root_font;
	void* root_boldfont;
	void* root_monofont;
	void* root_boldmonofont;

	int	  top_step;
	MwWidget* draw_queue;
	MwWidget* resize_queue;
	MwWidget* monitored_entries;

	int  berserk;
	long last_tick;

	MwBool is_menu;
};
#endif

struct _MwMenu {
	char*	 name;
	int	 keep;
	MwWidget wsub;
	MwMenu*	 sub;
	void*	 opaque;
	int	 cleaned;
};

struct _MwEntry {
	int	cursor;
	int	right;
	int	length;
	int	active;
	int	cursor_blink_timer;
	MwPoint mouse;
};

struct _MwViewport {
	MwWidget vscroll;
	MwWidget hscroll;
	MwWidget frame;
	MwWidget inframe;
	int	 vchanged;
	int	 hchanged;
};

struct _MwListBoxEntry {
	char**	   name;
	MwLLPixmap pixmap;
};

struct _MwListBox {
	MwWidget	vscroll;
	MwWidget	frame;
	MwListBoxEntry* list;
	unsigned long	click_time;
	int		pressed;
	int*		width;
	int*		alignment;
	int		changed;
};

struct _MwComboBox {
	char**	 list;
	int	 opened;
	MwWidget listbox;
};

struct _MwTreeViewEntry {
	char*		  label;
	MwLLPixmap	  pixmap;
	MwTreeViewEntry** tree;
	int		  opened;
	int		  selected;
	unsigned long	  click_time;
	MwTreeViewEntry*  parent;
};

struct _MwTreeView {
	MwWidget	  frame;
	MwWidget	  vscroll;
	int		  changed;
	MwTreeViewEntry** tree;

	MwPoint pressed;
};

struct _MwScrollBar {
	MwPoint point;
	int	drag;
	int	pos;
};

struct _MwLabel {
	MwLabelSegment* segment;
	MwHandler	draw;
};

struct _MwLabelSegment {
	int	      key;
	unsigned char value;
};

struct _MwDirectoryEntry {
	char*	 name;
	int	 type;
	MwOffset size;
	time_t	 mtime;
};

struct _MwBox {
	int layout;
};

struct _MwTable {
	int	  layout;
	MwWidget* widgets;
};

struct _MwSubWindow {
	MwWidget frame;
	MwWidget minimize;
	MwWidget maximize;
	MwWidget close;
	int	 maximized;
	int	 minimized;
	int	 x;
	int	 y;
	int	 width;
	int	 height;
	MwPoint	 cursor_start;
	MwPoint	 base;
};

struct _MwTab {
	MwWidget* frames;
	char**	  names;
};

struct _MwMouse {
	MwPoint point;
	int	button;
};

struct _MwTTFInfo {
	char* family;
	int   weight;
};

struct _MwClass {
	MwHandlerWithStatus	   create;
	MwHandler		   destroy;
	MwHandler		   draw;
	MwHandler		   click;
	MwHandler		   parent_resize;
	MwHandlerProp		   prop_change;
	MwHandler		   mouse_move;
	MwHandlerMouse		   mouse_up;
	MwHandlerMouse		   mouse_down;
	MwHandlerKey		   key;
	MwHandlerExecute	   execute;
	MwHandler		   tick;
	MwHandler		   resize;
	MwHandlerChildrenUpdate	   children_update;
	MwHandlerChildrenProp	   children_prop_change;
	MwHandlerClipboardReceived clipboard;
	MwHandlerProps		   props_change;
	void*			   reserved1;
	void*			   reserved2;
	void*			   reserved3;
};

#endif
