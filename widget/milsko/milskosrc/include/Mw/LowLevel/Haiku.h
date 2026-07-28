/*!
 * @file Mw/LowLevel/Haiku.h
 * @brief Haiku Backend
 * @warning This is used internally
 */
#ifndef __MW_LOWLEVEL_HAIKU_H__
#define __MW_LOWLEVEL_HAIKU_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/LowLevel.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
class MwApplication;
class MwView;
class MwWindow;

class MwApplication : public BApplication {
    public:
	MwApplication(MwRect rc, MwLL handle);
	~MwApplication();

	void MessageReceived(BMessage* message);
};

class MwView : public BView {
    public:
	MwLL	 handle;
	BLocker* locker;
	uint32	 buttons;
	BBitmap* bitmap;

	MwView(MwLL handle, BRect rc, uint32 resizingMode, uint32 flags);
	~MwView();

	void	 MessageReceived(BMessage* message);
	void	 PostMessage(BMessage* message);
	void	 PostMessage(uint32 command);
	status_t SendMessage(BMessage* message);
	status_t SendMessage(uint32 command);
	void	 Invalidate();

	void AttachedToWindow();
	void Draw(BRect updateRect);
	void FrameResized(float width, float height);
	void MouseDown(BPoint point);
	void MouseUp(BPoint point);
	void MouseMoved(BPoint point, uint32 transit, const BMessage* message);

	void SetColor(MwLLColor color);
};

class MwWindow : public BWindow {
    public:
	MwWindow(BRect rc, window_type type, uint32 flags);

	void MessageReceived(BMessage* message);

	void FrameMoved(BPoint newLocation);
	bool QuitRequested();
};

typedef BBitmap MwBBitmap;
#else
typedef void MwApplication;
typedef void MwView;
typedef void MwWindow;
typedef void MwBBitmap;
#endif

typedef union _MwLLHaikuEvent MwLLHaikuEvent;

enum BAPP_MW_WHAT {
	BAPP_MW_DESTROY = 0
};

enum BVIEW_MW_WHAT {
	BVIEW_MW_DESTROY = 0,
	BVIEW_MW_DETACH,
	BVIEW_MW_RESIZE,
	BVIEW_MW_MOVE,
	BVIEW_MW_SHOW,
	BVIEW_MW_HIDE,
	BVIEW_MW_SET_COLOR,
	BVIEW_MW_POLYGON,
	BVIEW_MW_LINE,
	BVIEW_MW_BITMAP,
	BVIEW_MW_RENDER,
	BVIEW_MW_RAISE,
	BVIEW_MW_GET_MOUSE
};

enum BWIN_MW_WHAT {
	BWIN_MW_DESTROY = 0,
	BWIN_MW_SET_TITLE,
	BWIN_MW_SET_TYPE
};

enum MwLLHAIKU_EVENT {
	MwLLHAIKU_EVENT_DRAW = 0,
	MwLLHAIKU_EVENT_CLOSE,
	MwLLHAIKU_EVENT_FRAMERESIZED,
	MwLLHAIKU_EVENT_FRAMEMOVED,
	MwLLHAIKU_EVENT_MOUSEDOWN,
	MwLLHAIKU_EVENT_MOUSEUP,
	MwLLHAIKU_EVENT_MOUSEMOVED
};

struct _MwLLHaikuEventMouse {
	int	type;
	MwPoint point;
	int	button;
};

struct _MwLLHaikuEventFrameResized {
	int   type;
	float width;
	float height;
};

struct _MwLLHaikuEventFrameMoved {
	int	type;
	MwPoint point;
};

union _MwLLHaikuEvent {
	int				   type;
	struct _MwLLHaikuEventMouse	   mouse;
	struct _MwLLHaikuEventFrameResized frame_resized;
	struct _MwLLHaikuEventFrameMoved   frame_moved;
};

struct _MwLLHaiku {
	struct _MwLLCommon common;

	int type;

	int x;
	int y;
	int width;
	int height;

	MwLL	       parent;
	MwApplication* app;
	MwWindow*      window;
	MwView*	       view;
	thread_id      app_thread;
	int	       force_render;

	MwLLHaikuEvent* events;
};

struct _MwLLHaikuColor {
	struct _MwLLCommonColor common;
};

struct _MwLLHaikuPixmap {
	struct _MwLLCommonPixmap common;

	MwBBitmap* bitmap;
};

MWDECL int MwLLHaikuCallInit(void);

#ifdef __cplusplus
}
#endif

#endif
