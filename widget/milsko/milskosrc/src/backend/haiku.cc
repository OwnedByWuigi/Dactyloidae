#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

MwApplication::MwApplication(MwRect rc, MwLL handle)
    : BApplication("application/milsko-generic") {
	BScreen* scr = new BScreen();
	float	 x   = (scr->Frame().Width() - rc.width) / 2;
	float	 y   = (scr->Frame().Height() - rc.height) / 2;
	BRect	 rc2;
	int	 dx = rc.x == MwDEFAULT;
	int	 dy = rc.y == MwDEFAULT;

	rc.x += dx ? -rc.x : 0;
	rc.y += dy ? -rc.y : 0;

	if(dx) rc.x += x;
	if(dy) rc.y += y;

	rc2 = BRect(rc.x, rc.y, rc.x + rc.width - 1, rc.y + rc.height - 1);

	handle->haiku.window = new MwWindow(rc2, B_TITLED_WINDOW, 0);
	handle->haiku.window->Show();

	handle->haiku.view = new MwView(handle, handle->haiku.window->Bounds(), B_FOLLOW_ALL_SIDES, B_WILL_DRAW);

	handle->haiku.window->AddChild(handle->haiku.view);

	delete scr;
}

MwApplication::~MwApplication() {
}

void MwApplication::MessageReceived(BMessage* message) {
	switch(message->what) {
	case BAPP_MW_DESTROY:
	{
		this->Quit();
		break;
	}
	default:
	{
		BLooper::MessageReceived(message);
		break;
	}
	}
}

MwView::MwView(MwLL handle, BRect frame, uint32 resizingMode, uint32 flags)
    : BView(frame, NULL, resizingMode, flags | B_FRAME_EVENTS) {
	this->handle = handle;

	this->locker = new BLocker();

	this->SetDrawingMode(B_OP_OVER);
	this->SetLineMode(B_BUTT_CAP, B_BUTT_JOIN);

	this->SetViewColor(B_TRANSPARENT_COLOR);

	this->buttons = 0;
}

MwView::~MwView() {
	delete this->locker;
}

void MwView::MessageReceived(BMessage* message) {
	switch(message->what) {
	case BVIEW_MW_DESTROY:
	{
		this->RemoveSelf();
		delete this;
		break;
	}
	case BVIEW_MW_DETACH:
	{
		BRect  rc;
		BPoint point;

		if(message->FindPoint("view-point", &point) != B_OK) break;

		point = this->ConvertToScreen(this->ConvertFromParent(point));

		rc.left	  = point.x;
		rc.top	  = point.y;
		rc.right  = rc.left + this->handle->haiku.width - 1;
		rc.bottom = rc.top + this->handle->haiku.height - 1;

		if(this->Parent() != NULL) {
			this->RemoveSelf();
		}

		this->handle->haiku.parent = NULL;

		this->handle->haiku.window = new MwWindow(rc, B_TITLED_WINDOW, 0);
		this->handle->haiku.window->Show();

		this->MoveTo(0, 0);
		this->ResizeTo(this->handle->haiku.width, this->handle->haiku.height);
		this->SetResizingMode(B_FOLLOW_ALL_SIDES);
		this->handle->haiku.window->AddChild(this);

		while(this->Window() != this->handle->haiku.window);

		MwLLForceRender(this->handle);
		break;
	}
	case BVIEW_MW_RESIZE:
	{
		int width, height;

		if(message->FindInt32("view-width", &width) != B_OK) break;
		if(message->FindInt32("view-height", &height) != B_OK) break;

		if(this->handle->haiku.window == NULL) {
			this->ResizeTo(width - 1, height - 1);
		} else {
			this->handle->haiku.window->ResizeTo(width - 1, height - 1);
			this->ResizeTo(width - 1, height - 1);
		}
		this->Draw(BRect(0, 0, 0, 0));
		break;
	}
	case BVIEW_MW_MOVE:
	{
		int x, y;

		if(message->FindInt32("view-x", &x) != B_OK) break;
		if(message->FindInt32("view-y", &y) != B_OK) break;

		if(this->handle->haiku.app == NULL) {
			this->MoveTo(x, y);
		} else {
			this->Window()->MoveTo(x, y);
		}
		break;
	}
	case BVIEW_MW_SHOW:
	{
		if(this->Parent() == NULL) {
			this->handle->haiku.parent->haiku.view->AddChild(this);
		}
		break;
	}
	case BVIEW_MW_HIDE:
	{
		if(this->Parent() != NULL) {
			this->RemoveSelf();
		}
		break;
	}
	case BVIEW_MW_SET_COLOR:
	{
		int32 rgb[3];
		int   i;

		rgb[0] = rgb[1] = rgb[2] = 0;

		for(i = 0; i < 3 && message->FindInt32("view-color", i, &rgb[i]) == B_OK; i++);

		this->SetHighColor(rgb[0], rgb[1], rgb[2]);

		break;
	}
	case BVIEW_MW_POLYGON:
	{
		int    i;
		BPoint p;
		BPoint points[32];

		for(i = 0; i < 32 && message->FindPoint("view-point", i, &p) == B_OK; i++) {
			points[i] = p;
		}

		this->FillPolygon(points, i);
		break;
	}
	case BVIEW_MW_LINE:
	{
		int    i;
		BPoint p;
		BPoint points[2];

		for(i = 0; i < 2 && message->FindPoint("view-point", i, &p) == B_OK; i++) {
			points[i] = p;
		}

		this->StrokeLine(points[0], points[1]);
		break;
	}
	case BVIEW_MW_BITMAP:
	{
		BRect	 rc;
		BBitmap* bitmap;

		if(message->FindRect("view-rect", &rc) != B_OK) break;
		if(message->FindPointer("view-bitmap", (void**)&bitmap) != B_OK) break;

		this->DrawBitmap(bitmap, rc);
		break;
	}
	case BVIEW_MW_RENDER:
	{
		this->Invalidate();
		break;
	}
	case BVIEW_MW_RAISE:
	{
		BView* v = this->Parent();

		this->RemoveSelf();
		v->AddChild(this);
		break;
	}
	case BVIEW_MW_GET_MOUSE:
	{
		BPoint* p;
		uint32	btn;

		if(message->FindPointer("view-ppoint", (void**)&p) != B_OK) break;

		this->GetMouse(p, &btn);

		break;
	}
	default:
	{
		BView::MessageReceived(message);
		break;
	}
	}
}

void MwView::PostMessage(BMessage* message) {
	BMessage copy = *message;
	MwLL	 top  = this->handle;

	while(top->haiku.parent != NULL) top = top->haiku.parent;

	copy.AddPointer("window-view", this);

	top->haiku.window->PostMessage(&copy);
}

void MwView::PostMessage(uint32 command) {
	BMessage msg(command);

	this->PostMessage(&msg);
}

status_t MwView::SendMessage(BMessage* message) {
	BMessage copy = *message;
	BMessage reply;
	MwLL	 top = this->handle;

	while(top->haiku.parent != NULL) top = top->haiku.parent;

	copy.AddPointer("window-view", this);

	return BMessenger(top->haiku.window).SendMessage(&copy, &reply);
}

status_t MwView::SendMessage(uint32 command) {
	BMessage msg(command);

	return this->SendMessage(&msg);
}

void MwView::Invalidate() {
	this->Draw(BRect(0, 0, 0, 0));
}

void MwView::AttachedToWindow() {
	this->SetOrigin(0, 0);
}

void MwView::Draw(BRect updateRect) {
	MwLLHaikuEvent ev;

	(void)updateRect;

	ev.type = MwLLHAIKU_EVENT_DRAW;

	this->locker->Lock();
	arrput(this->handle->haiku.events, ev);
	this->locker->Unlock();
}

void MwView::FrameResized(float width, float height) {
	MwLLHaikuEvent ev;

	ev.type			= MwLLHAIKU_EVENT_FRAMERESIZED;
	ev.frame_resized.width	= width;
	ev.frame_resized.height = height;

	this->locker->Lock();
	arrput(this->handle->haiku.events, ev);
	this->locker->Unlock();
}

void MwView::MouseDown(BPoint point) {
	MwLLHaikuEvent ev;
	uint32	       btn;
	BPoint	       p;

	this->GetMouse(&p, &btn, false);

	this->SetMouseEventMask(B_LOCK_WINDOW_FOCUS);

	ev.type		 = MwLLHAIKU_EVENT_MOUSEDOWN;
	ev.mouse.point.x = point.x;
	ev.mouse.point.y = point.y;

	if((btn & B_PRIMARY_MOUSE_BUTTON) && !(this->buttons & B_PRIMARY_MOUSE_BUTTON)) {
		ev.mouse.button = MwMOUSE_LEFT;
	} else if((btn & B_SECONDARY_MOUSE_BUTTON) && !(this->buttons & B_SECONDARY_MOUSE_BUTTON)) {
		ev.mouse.button = MwMOUSE_RIGHT;
	} else if((btn & B_TERTIARY_MOUSE_BUTTON) && !(this->buttons & B_TERTIARY_MOUSE_BUTTON)) {
		ev.mouse.button = MwMOUSE_MIDDLE;
	}
	this->buttons = btn;

	this->locker->Lock();
	arrput(this->handle->haiku.events, ev);
	this->locker->Unlock();
}

void MwView::MouseUp(BPoint point) {
	MwLLHaikuEvent ev;
	uint32	       btn;
	BPoint	       p;

	this->GetMouse(&p, &btn, false);

	ev.type		 = MwLLHAIKU_EVENT_MOUSEUP;
	ev.mouse.point.x = point.x;
	ev.mouse.point.y = point.y;

	if(!(btn & B_PRIMARY_MOUSE_BUTTON) && (this->buttons & B_PRIMARY_MOUSE_BUTTON)) {
		ev.mouse.button = MwMOUSE_LEFT;
	} else if(!(btn & B_SECONDARY_MOUSE_BUTTON) && (this->buttons & B_SECONDARY_MOUSE_BUTTON)) {
		ev.mouse.button = MwMOUSE_RIGHT;
	} else if(!(btn & B_TERTIARY_MOUSE_BUTTON) && (this->buttons & B_TERTIARY_MOUSE_BUTTON)) {
		ev.mouse.button = MwMOUSE_MIDDLE;
	}
	this->buttons = btn;

	this->locker->Lock();
	arrput(this->handle->haiku.events, ev);
	this->locker->Unlock();
}

void MwView::MouseMoved(BPoint point, uint32 transit, const BMessage* message) {
	MwLLHaikuEvent ev;

	(void)transit;
	(void)message;

	ev.type		 = MwLLHAIKU_EVENT_MOUSEMOVED;
	ev.mouse.point.x = point.x;
	ev.mouse.point.y = point.y;
	ev.mouse.button	 = 0;

	this->locker->Lock();
	arrput(this->handle->haiku.events, ev);
	this->locker->Unlock();
}

void MwView::SetColor(MwLLColor color) {
	BMessage msg(BVIEW_MW_SET_COLOR);

	msg.AddInt32("view-color", color->common.red);
	msg.AddInt32("view-color", color->common.green);
	msg.AddInt32("view-color", color->common.blue);

	this->PostMessage(&msg);
}

MwWindow::MwWindow(BRect frame, window_type type, uint32 flags)
    : BWindow(frame, "Milsko", type, flags) {
}

void MwWindow::MessageReceived(BMessage* message) {
	void* ptr;

	if(message->FindPointer("window-view", &ptr) == B_OK) {
		MwView* v = (MwView*)ptr;

		v->MessageReceived(message);

		return;
	}

	switch(message->what) {
	case BVIEW_MW_DESTROY:
	{
		this->Quit();
		delete this;
		break;
	}
	case BWIN_MW_SET_TITLE:
	{
		const char* title;

		if(message->FindString("window-title", &title) != B_OK) break;

		this->SetTitle(title);
		break;
	}
	case BWIN_MW_SET_TYPE:
	{
		int type;

		if(message->FindInt32("window-type", &type) != B_OK) break;

		this->SetType((window_type)type);

		if(type == B_UNTYPED_WINDOW) {
			this->SetLook(B_NO_BORDER_WINDOW_LOOK);
		}
		break;
	}
	default:
	{
		BWindow::MessageReceived(message);
		break;
	}
	}
}

void MwWindow::FrameMoved(BPoint newLocation) {
	MwLLHaikuEvent ev;
	MwView*	       v = (MwView*)this->ChildAt(0);

	ev.type		       = MwLLHAIKU_EVENT_FRAMEMOVED;
	ev.frame_moved.point.x = newLocation.x;
	ev.frame_moved.point.y = newLocation.y;

	v->locker->Lock();
	arrput(v->handle->haiku.events, ev);
	v->locker->Unlock();
}

bool MwWindow::QuitRequested() {
	MwLLHaikuEvent ev;
	MwView*	       v = (MwView*)this->ChildAt(0);

	ev.type = MwLLHAIKU_EVENT_CLOSE;

	v->locker->Lock();
	arrput(v->handle->haiku.events, ev);
	v->locker->Unlock();

	return false;
}

struct app_param {
	MwRect rc;
	MwLL   handle;
};

static int32 app_thread(void* data) {
	struct app_param* r = (struct app_param*)data;

	r->handle->haiku.app = new MwApplication(r->rc, r->handle);
	r->handle->haiku.app->Run();

	free(r);

	return 0;
}

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL   r = (MwLL)malloc(sizeof(*r));
	MwRect mrc;
	BRect  rc = BRect(x, y, x + width - 1, y + height - 1);

	mrc.x	   = x;
	mrc.y	   = y;
	mrc.width  = width;
	mrc.height = height;

	MwLLCreateCommon(r);

	r->common.copy_buffer = 1;
	r->common.type	      = MwLLBackendHaiku;

	r->haiku.force_render = 0;

	r->haiku.parent = parent;

	r->haiku.events = NULL;

	if(parent == NULL) {
		struct app_param* p = (struct app_param*)malloc(sizeof(*p));

		r->haiku.app = NULL;

		p->rc	  = mrc;
		p->handle = r;

		r->haiku.app	= NULL;
		r->haiku.window = NULL;
		r->haiku.view	= NULL;

		r->haiku.app_thread = spawn_thread(app_thread, NULL, B_NORMAL_PRIORITY, p);
		resume_thread(r->haiku.app_thread);

		while(r->haiku.app == NULL || r->haiku.window == NULL || r->haiku.view == NULL) MwTimeSleep(10);
	} else {
		MwLL top = r;
		while(top->haiku.parent != NULL) {
			top = top->haiku.parent;
		}

		r->haiku.app	= NULL;
		r->haiku.window = NULL;
		r->haiku.view	= new MwView(r, rc, B_FOLLOW_NONE, B_WILL_DRAW);

		parent->haiku.view->AddChild(r->haiku.view);

		r->haiku.view->SendMessage(BVIEW_MW_RAISE);
	}

	r->haiku.x	= x;
	r->haiku.y	= y;
	r->haiku.width	= width;
	r->haiku.height = height;

	return r;
}

static void MwLLDestroyImpl(MwLL handle) {
	MwLLDestroyCommon(handle);

	handle->haiku.view->PostMessage(BVIEW_MW_DESTROY);

	if(handle->haiku.app != NULL) {
		status_t exit_value;

		handle->haiku.app->PostMessage(BAPP_MW_DESTROY);
		wait_for_thread(handle->haiku.app_thread, &exit_value);

		delete handle->haiku.app;
	} else if(handle->haiku.window != NULL) {
		handle->haiku.window->PostMessage(BWIN_MW_DESTROY);
	}

	free(handle);
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count, MwLLColor color) {
	int	 i;
	BMessage msg(BVIEW_MW_POLYGON);

	for(i = 0; i < points_count; i++) {
		BPoint p(points[i].x, points[i].y);

		msg.AddPoint("view-point", p);
	}

	handle->haiku.view->SetColor(color);
	handle->haiku.view->PostMessage(&msg);
}

static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	int	 i;
	BMessage msg(BVIEW_MW_LINE);

	for(i = 0; i < 2; i++) {
		BPoint p(points[i].x, points[i].y);

		msg.AddPoint("view-point", p);
	}

	handle->haiku.view->SetColor(color);
	handle->haiku.view->PostMessage(&msg);
}

static void MwLLBeginDrawImpl(MwLL handle) {
	(void)handle;
}

static void MwLLEndDrawImpl(MwLL handle) {
	(void)handle;
}

static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = (MwLLColor)malloc(sizeof(*c));

	MwLLColorUpdate(handle, c, r, g, b);

	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	(void)handle;

	c->common.red	= r;
	c->common.green = g;
	c->common.blue	= b;
}

static void MwLLFreeColorImpl(MwLLColor color) {
	free(color);
}

static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) {
	*x = handle->haiku.x;
	*y = handle->haiku.y;
	*w = handle->haiku.width;
	*h = handle->haiku.height;
}

static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	BMessage msg(BVIEW_MW_MOVE);

	handle->haiku.x = x;
	handle->haiku.y = y;

	msg.AddInt32("view-x", x);
	msg.AddInt32("view-y", y);

	handle->haiku.view->PostMessage(&msg);
}

static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	BMessage msg(BVIEW_MW_RESIZE);

	handle->haiku.width  = w;
	handle->haiku.height = h;

	msg.AddInt32("view-width", w);
	msg.AddInt32("view-height", h);

	handle->haiku.view->PostMessage(&msg);

	MwLLDispatch(handle, resize, NULL);
}

static void MwLLSetTitleImpl(MwLL handle, const char* title) {
	BMessage msg(BWIN_MW_SET_TITLE);

	if(handle->haiku.app == NULL) return;

	msg.AddString("window-title", title);

	handle->haiku.window->PostMessage(&msg);
}

static int MwLLPendingImpl(MwLL handle) {
	handle->haiku.view->locker->Lock();
	if(arrlen(handle->haiku.events) > 0) {
		handle->haiku.view->locker->Unlock();
		return 1;
	}
	handle->haiku.view->locker->Unlock();

	return 0;
}

static void MwLLNextEventImpl(MwLL handle) {
	int rendered = 0;

	handle->haiku.view->locker->Lock();
	while(arrlen(handle->haiku.events) > 0) {
		MwLLHaikuEvent* ev = &handle->haiku.events[0];
		MwMouse		m;

		if(ev->type == MwLLHAIKU_EVENT_MOUSEDOWN || ev->type == MwLLHAIKU_EVENT_MOUSEUP) {
			m.point	 = ev->mouse.point;
			m.button = ev->mouse.button;
		}

		if(ev->type == MwLLHAIKU_EVENT_DRAW) {
			MwLLDispatch(handle, draw, NULL);

			rendered = 1;
		} else if(ev->type == MwLLHAIKU_EVENT_CLOSE) {
			MwLLDispatch(handle, close, NULL);
		} else if(ev->type == MwLLHAIKU_EVENT_FRAMERESIZED) {
			handle->haiku.width  = ev->frame_resized.width + 1;
			handle->haiku.height = ev->frame_resized.height + 1;

			MwLLDispatch(handle, resize, NULL);
		} else if(ev->type == MwLLHAIKU_EVENT_FRAMEMOVED) {
			handle->haiku.x = ev->frame_moved.point.x + 1;
			handle->haiku.y = ev->frame_moved.point.y + 1;
		} else if(ev->type == MwLLHAIKU_EVENT_MOUSEDOWN) {
			MwLLDispatch(handle, down, &m);
		} else if(ev->type == MwLLHAIKU_EVENT_MOUSEUP) {
			MwLLDispatch(handle, up, &m);
		} else if(ev->type == MwLLHAIKU_EVENT_MOUSEMOVED) {
			MwLLDispatch(handle, move, &ev->mouse.point);
		}

		arrdel(handle->haiku.events, 0);
	}
	handle->haiku.view->locker->Unlock();

	if(rendered) handle->haiku.force_render = 0;
}

static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data, int width, int height) {
	MwLLPixmap r = (MwLLPixmap)malloc(sizeof(*r));

	(void)handle;

	r->common.raw = (unsigned char*)malloc(4 * width * height);
	memcpy(r->common.raw, data, 4 * width * height);

	r->common.width	 = width;
	r->common.height = height;

	r->haiku.bitmap = NULL;

	MwLLPixmapUpdate(r);
	return r;

	return r;
}

static void MwLLPixmapUpdateImpl(MwLLPixmap pixmap) {
	uint32* px = (uint32*)malloc(4 * pixmap->common.width * pixmap->common.height);
	int	i;

	for(i = 0; i < pixmap->common.width * pixmap->common.height; i++) {
		unsigned char* ipx = &pixmap->common.raw[4 * i];

		px[i] = (ipx[3] << 24) | (ipx[0] << 16) | (ipx[1] << 8) | (ipx[2] << 0);
	}

	if(pixmap->haiku.bitmap != NULL) delete pixmap->haiku.bitmap;

	pixmap->haiku.bitmap = new BBitmap(BRect(0, 0, pixmap->common.width - 1, pixmap->common.height - 1), B_RGBA32);

	pixmap->haiku.bitmap->SetBits(px, pixmap->common.width * pixmap->common.height * 4, 0, B_RGBA32);

	free(px);
}

static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
	free(pixmap->common.raw);

	free(pixmap);
}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
	BRect	 rc;
	BMessage msg(BVIEW_MW_BITMAP);

	rc = BRect(rect->x, rect->y, rect->x + rect->width - 1, rect->y + rect->height - 1);

	msg.AddRect("view-rect", rc);
	msg.AddPointer("view-bitmap", pixmap->haiku.bitmap);

	handle->haiku.view->PostMessage(&msg);
}

static void MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {
	(void)handle;
	(void)pixmap;
}

static void MwLLForceRenderImpl(MwLL handle) {
	if(handle->haiku.force_render) return;

	handle->haiku.force_render = 1;
	handle->haiku.view->PostMessage(BVIEW_MW_RENDER);
}

static void MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {
	(void)handle;
	(void)image;
	(void)mask;
}

static void MwLLDetachImpl(MwLL handle, MwPoint* point) {
	BMessage msg(BVIEW_MW_DETACH);

	msg.AddPoint("view-point", BPoint(point->x, point->y));

	handle->haiku.view->PostMessage(&msg);

	while(handle->haiku.window == NULL);
}

static void MwLLShowImpl(MwLL handle, int show) {
	if(show) {
		handle->haiku.view->PostMessage(BVIEW_MW_SHOW);
	} else {
		handle->haiku.view->PostMessage(BVIEW_MW_HIDE);
	}
}

static void MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx, int maxy) {
	(void)handle;
	(void)minx;
	(void)miny;
	(void)maxx;
	(void)maxy;
}

static void MwLLMakeBorderlessImpl(MwLL handle, int toggle) {
	if(toggle) {
		handle->haiku.type = B_TITLED_WINDOW;
	} else {
		handle->haiku.type = B_UNTYPED_WINDOW;
	}
}

static void MwLLMakeToolWindowImpl(MwLL handle) {
	handle->haiku.type = B_UNTYPED_WINDOW;
}

static void MwLLMakePopupImpl(MwLL handle, MwLL parent) {
	(void)handle;
	(void)parent;
}

static void MwLLBeginStateChangeImpl(MwLL handle) {
	handle->haiku.type = handle->haiku.window == NULL ? B_TITLED_WINDOW : handle->haiku.window->Type();
}

static void MwLLEndStateChangeImpl(MwLL handle) {
	BMessage msg(BWIN_MW_SET_TYPE);

	msg.AddInt32("window-type", handle->haiku.type);

	BMessenger(handle->haiku.window).SendMessage(&msg);
}

static void MwLLFocusImpl(MwLL handle) {
	(void)handle;
}

static void MwLLGrabPointerImpl(MwLL handle, int toggle) {
	(void)handle;
	(void)toggle;
}

static void MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {
	(void)handle;
	(void)text;
	(void)clipboard_type;
}

static void MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {
	(void)handle;
	(void)clipboard_type;
}

static void MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {
	MwLL	 top = handle;
	BMessage msg(BVIEW_MW_GET_MOUSE);
	BPoint	 p;
	while(top->haiku.parent != NULL) {
		top = top->haiku.parent;
	}

	msg.AddPointer("view-ppoint", &p);
	top->haiku.view->SendMessage(&msg);

	point->x = p.x;
	point->y = p.y;
}

static void MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {
	BScreen* screen = new BScreen();

	(void)handle;

	if(screen->IsValid()) {
		rect->width  = screen->Frame().Width();
		rect->height = screen->Frame().Height();
	}

	delete screen;
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
	handle->haiku.view->SendMessage(BVIEW_MW_RAISE);
}

static void MwLLClipImpl(MwLL handle, MwRect* rect) {
	(void)handle;
	(void)rect;
}

int MwLLHaikuCallInitImpl() {
	return 0;
}

#include "call.c"
CALL(Haiku);
