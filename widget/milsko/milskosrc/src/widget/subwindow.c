#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

#define TitleHeight 20
#define ButtonSize 14

static void resize(MwWidget handle);

static void close_draw(MwWidget handle) {
	int	  w = MwGetInteger(handle, MwNwidth);
	int	  h = MwGetInteger(handle, MwNheight);
	MwPoint	  p[2];
	MwLLColor c  = MwParseColor(handle, MwGetText(handle, MwNforeground));
	int	  bw = MwDefaultBorderWidth(handle);

	p[0].x = bw * 2;
	p[0].y = bw * 2;
	p[1].x = w - bw * 2 - 1;
	p[1].y = h - bw * 2 - 1;
	MwLLLine(handle->lowlevel, p, c);

	p[0].x++;
	p[1].x--;
	MwLLLine(handle->lowlevel, p, c);

	p[0].x = w - bw * 2 - 1;
	p[0].y = bw * 2;
	p[1].x = bw * 2;
	p[1].y = h - bw * 2 - 1;
	MwLLLine(handle->lowlevel, p, c);

	p[0].x--;
	p[1].x++;
	MwLLLine(handle->lowlevel, p, c);

	MwLLFreeColor(c);
}

static void MWAPI close_activate(MwWidget handle, void* user, void* client) {
	MwWidget w = user;

	(void)handle;
	(void)client;

	MwDispatchUserHandler(w, MwNcloseHandler, NULL);

	MwLLRaise(w->lowlevel);
}

static void maximize_draw(MwWidget handle) {
	int	  w = MwGetInteger(handle, MwNwidth);
	int	  h = MwGetInteger(handle, MwNheight);
	MwPoint	  p[2];
	MwLLColor c  = MwParseColor(handle, MwGetText(handle, MwNforeground));
	int	  bw = MwDefaultBorderWidth(handle);

	p[0].x = bw * 2;
	p[0].y = bw * 2;
	p[1].x = w - bw * 2 - 1;
	p[1].y = bw * 2;
	MwLLLine(handle->lowlevel, p, c);

	p[0].y = p[1].y = h - bw * 2 - 1;
	MwLLLine(handle->lowlevel, p, c);

	p[0].y = p[1].y = bw * 2 + 1;
	MwLLLine(handle->lowlevel, p, c);

	p[0].x = bw * 2;
	p[0].y = bw * 2;
	p[1].x = bw * 2;
	p[1].y = h - bw * 2 - 1;
	MwLLLine(handle->lowlevel, p, c);

	p[0].x = p[1].x = w - bw * 2 - 1;
	MwLLLine(handle->lowlevel, p, c);

	MwLLFreeColor(c);
}

static void MWAPI maximize_activate(MwWidget handle, void* user, void* client) {
	MwWidget    w  = user;
	MwSubWindow sw = w->internal;

	(void)handle;
	(void)client;

	if(sw->minimized) return;

	if(sw->maximized) {
		MwVaApply(user,
			  MwNx, sw->x,
			  MwNy, sw->y,
			  MwNwidth, sw->width,
			  MwNheight, sw->height,
			  NULL);
	} else {
		sw->x	   = MwGetInteger(w, MwNx);
		sw->y	   = MwGetInteger(w, MwNy);
		sw->width  = MwGetInteger(w, MwNwidth);
		sw->height = MwGetInteger(w, MwNheight);

		MwVaApply(user,
			  MwNx, 0,
			  MwNy, 0,
			  MwNwidth, MwGetInteger(w->parent, MwNwidth),
			  MwNheight, MwGetInteger(w->parent, MwNheight),
			  NULL);
	}
	sw->maximized = !sw->maximized;

	MwLLRaise(w->lowlevel);
}

static void minimize_draw(MwWidget handle) {
	int	  w = MwGetInteger(handle, MwNwidth);
	int	  h = MwGetInteger(handle, MwNheight);
	MwPoint	  p[2];
	MwLLColor c  = MwParseColor(handle, MwGetText(handle, MwNforeground));
	int	  bw = MwDefaultBorderWidth(handle);

	p[0].x = bw * 2;
	p[0].y = h - bw * 2 - 1;
	p[1].x = w - bw * 2 - 1;
	p[1].y = h - bw * 2 - 1;
	MwLLLine(handle->lowlevel, p, c);

	p[0].y--;
	p[1].y--;
	MwLLLine(handle->lowlevel, p, c);

	MwLLFreeColor(c);
}

static int sort_subwindow(const void* _a, const void* _b) {
	MwWidget a  = *(MwWidget*)_a;
	MwWidget b  = *(MwWidget*)_b;
	int	 ax = MwGetInteger(a, MwNx);
	int	 bx = MwGetInteger(b, MwNx);

	if(ax < bx) return -1;
	if(ax > bx) return 1;
	return 0;
}

static void MWAPI minimize_activate(MwWidget handle, void* user, void* client) {
	MwWidget    w  = user;
	MwSubWindow sw = w->internal;

	(void)handle;
	(void)client;

	if(sw->maximized) return;

	if(sw->minimized) {
		int	  i;
		MwWidget* ws = NULL;
		int	  p  = 0;

		for(i = 0; i < arrlen(w->parent->children); i++) {
			MwWidget c = w->parent->children[i];
			if(c->widget_class == MwSubWindowClass) {
				MwSubWindow csw = c->internal;

				if(!csw->minimized) continue;

				arrput(ws, c);
			}
		}

		if(ws != NULL) {
			qsort(ws, arrlen(ws), sizeof(*ws), sort_subwindow);

			for(i = 0; i < arrlen(ws); i++) {
				if(p) {
					MwVaApply(ws[i],
						  MwNx, MwGetInteger(ws[i], MwNx) - MwGetInteger(w, MwNwidth),
						  NULL);
				} else if(ws[i] == w) {
					p = 1;
				}
			}
		}
		arrfree(ws);

		MwVaApply(user,
			  MwNx, sw->x,
			  MwNy, sw->y,
			  NULL);
	} else {
		int x = 0;
		int i;

		for(i = 0; i < arrlen(w->parent->children); i++) {
			MwWidget c = w->parent->children[i];

			if(c->widget_class == MwSubWindowClass) {
				MwSubWindow cw = c->internal;

				if(cw->minimized) x += MwGetInteger(c, MwNwidth);
			}
		}

		sw->x = MwGetInteger(w, MwNx);
		sw->y = MwGetInteger(w, MwNy);

		MwVaApply(user,
			  MwNx, x,
			  MwNy, MwGetInteger(w->parent, MwNheight) - TitleHeight - MwDefaultBorderWidth(handle),
			  NULL);
	}
	sw->minimized = !sw->minimized;

	MwLLRaise(w->lowlevel);
}

static void resize(MwWidget handle) {
	MwSubWindow sw = handle->internal;
	int	    x, y, w, h;
	int	    ww = MwGetInteger(handle, MwNwidth);
	int	    wh = MwGetInteger(handle, MwNheight);
	int	    bw = MwDefaultBorderWidth(handle);
	int	    p;

	x = bw * 2;
	y = bw * 2 + TitleHeight;
	w = ww - bw * 4;
	h = wh - bw * 4 - TitleHeight;
	if(sw->frame == NULL) {
		sw->frame = MwCreateWidget(MwFrameClass, "frame", handle, x, y, w, h);
	} else {
		MwVaApply(sw->frame,
			  MwNx, x,
			  MwNy, y,
			  MwNwidth, w,
			  MwNheight, h,
			  NULL);
	}

	p = (TitleHeight - ButtonSize) / 2;
	x = ww - bw - p;
	y = bw + p;
	w = ButtonSize;
	h = ButtonSize;

#define BUTTON(name) \
	x -= ButtonSize; \
	if(sw->name == NULL) { \
		sw->name	      = MwCreateWidget(MwButtonClass, #name, handle, x, y, w, h); \
		sw->name->draw_inject = name##_draw; \
		MwAddUserHandler(sw->name, MwNactivateHandler, name##_activate, handle); \
	} else { \
		MwVaApply(sw->name, \
			  MwNx, x, \
			  MwNy, y, \
			  MwNwidth, w, \
			  MwNheight, h, \
			  NULL); \
	} \
	x -= p;

	BUTTON(close);
	BUTTON(maximize);
	BUTTON(minimize);
}

static int wcreate(MwWidget handle) {
	MwSubWindow sw = malloc(sizeof(*sw));
	memset(sw, 0, sizeof(*sw));

	sw->maximized	 = 0;
	sw->minimized	 = 0;
	handle->internal = sw;

	resize(handle);

	MwSetDefault(handle);

	return 0;
}

static void destroy(MwWidget handle) {
	free(handle->internal);
}

static void draw(MwWidget handle) {
	MwLLColor   c  = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor   tb = MwParseColor(handle, MwGetText(handle, MwNtitleBackground));
	MwLLColor   tf = MwParseColor(handle, MwGetText(handle, MwNtitleForeground));
	MwRect	    r, r2, r3;
	const char* title = MwGetText(handle, MwNtitle);
	int	    incr  = 0;
	MwLLPixmap  px	  = MwGetVoid(handle, MwNiconPixmap);

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawFrame(handle, &r, c, 0);
	MwDrawRect(handle, &r, c);

	r2	  = r;
	r2.height = TitleHeight;
	MwDrawRect(handle, &r2, tb);

	if(px != NULL) {
		r3   = r2;
		incr = (TitleHeight - ButtonSize) / 2;

		r3.x += incr;
		r3.y += incr;
		r3.width  = ButtonSize;
		r3.height = ButtonSize;
		MwLLDrawPixmap(handle->lowlevel, &r3, px);

		incr += ButtonSize;
	}

	if(title != NULL) {
		MwPoint p;
		p.x		= r2.x + (TitleHeight - ButtonSize) / 2 + incr;
		p.y		= r2.y + TitleHeight / 2;
		handle->bgcolor = tb;
		MwDrawText(handle, NULL, &p, title, MwALIGNMENT_BEGINNING, tf);
		handle->bgcolor = NULL;
	}

	r2.width = ButtonSize * 3 + (TitleHeight - ButtonSize) / 2 * 4;
	r2.x	 = r.width - r2.width;
	MwDrawRect(handle, &r2, tb);

	r.y += TitleHeight;
	r.height -= TitleHeight;
	MwDrawFrame(handle, &r, c, 1);

	MwLLFreeColor(tf);
	MwLLFreeColor(tb);
	MwLLFreeColor(c);
}

static void parent_resize(MwWidget handle) {
	MwSubWindow sw = handle->internal;

	if(sw->maximized) {
		MwVaApply(handle,
			  MwNwidth, MwGetInteger(handle->parent, MwNwidth),
			  MwNheight, MwGetInteger(handle->parent, MwNheight),
			  NULL);
	} else if(sw->minimized) {
		MwVaApply(handle,
			  MwNy, MwGetInteger(handle->parent, MwNheight) - TitleHeight - MwDefaultBorderWidth(handle),
			  NULL);
	}
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNtitle) == 0 || strcmp(key, MwNiconPixmap) == 0) MwForceRender(handle);
}

static MwWidget mwSubWindowGetFrameImpl(MwWidget handle) {
	MwSubWindow sw = handle->internal;

	return sw->frame;
}

static void mouse_down(MwWidget handle, void* ptr) {
	MwSubWindow sw = handle->internal;
	MwMouse*    m  = ptr;

	if(m->button == MwMOUSE_LEFT && !sw->minimized && !sw->maximized) {
		MwGetCursorCoord(handle, &sw->cursor_start);
		sw->base.x = MwGetInteger(handle, MwNx);
		sw->base.y = MwGetInteger(handle, MwNy);
	}

	MwLLRaise(handle->lowlevel);
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	(void)va;

	if(strcmp(name, "mwSubWindowGetFrame") == 0) {
		*(MwWidget*)out = mwSubWindowGetFrameImpl(handle);
	}
}

static void tick(MwWidget handle) {
	MwSubWindow sw = handle->internal;

	if(handle->pressed && !sw->minimized && !sw->maximized) {
		MwPoint p;

		MwGetCursorCoord(handle, &p);

		MwVaApply(handle,
			  MwNx, sw->base.x + p.x - sw->cursor_start.x,
			  MwNy, sw->base.y + p.y - sw->cursor_start.y,
			  NULL);
	}
}

MwClassRec MwSubWindowClassRec = {
    wcreate,	   /* create */
    destroy,	   /* destroy */
    draw,	   /* draw */
    NULL,	   /* click */
    parent_resize, /* parent_resize */
    prop_change,   /* prop_change */
    NULL,	   /* mouse_move */
    NULL,	   /* mouse_up */
    mouse_down,	   /* mouse_down */
    NULL,	   /* key */
    func_handler,  /* execute */
    tick,	   /* tick */
    resize,	   /* resize */
    NULL,	   /* children_update */
    NULL,	   /* children_prop_change */
    NULL,	   /* clipboard */
    NULL,	   /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwSubWindowClass = &MwSubWindowClassRec;
