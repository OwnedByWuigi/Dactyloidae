#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

static void resize(MwWidget handle);

static int wcreate(MwWidget handle) {
	MwTab t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));

	handle->internal = t;

	MwSetDefault(handle);

	MwSetInteger(handle, MwNvalue, -1);

	return 0;
}

static void destroy(MwWidget handle) {
	MwTab t = handle->internal;
	int   i;

	for(i = 0; i < arrlen(t->names); i++) {
		free(t->names[i]);
	}

	arrfree(t->names);
	arrfree(t->frames);

	free(handle->internal);
}

static int tab_width(MwWidget handle, const char* text) {
	return MwTextWidth(handle, NULL, text) + 20;
}

static int tab_height(MwWidget handle) {
	return MwTextHeight(handle, NULL, "M") + 10;
}

static int icon_width(MwWidget handle, const char* text) {
	MwTab t = handle->internal;
	int   i;

	for(i = 0; i < arrlen(t->names); i++) {
		if(strcmp(t->names[i], text) == 0) {
			MwLLPixmap px = MwGetVoid(t->frames[i], MwNiconPixmap);

			if(px == NULL) return 0;

			return px->common.width * tab_height(handle) / px->common.height;
		}
	}

	return 0;
}

static void draw(MwWidget handle) {
	MwTab	  t  = handle->internal;
	MwLLColor c  = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor ct = MwParseColor(handle, MwGetText(handle, MwNforeground));
	MwRect	  r, r2;
	int	  h = tab_height(handle);
	int	  i;
	int	  n;
	int	  x = 0;

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawRect(handle, &r, c);

	for(n = 0; n < 2; n++) {
		x = 0;
		for(i = 0; i < arrlen(t->names); i++) {
			int render = 0;
			int iw	   = icon_width(handle, t->names[i]);

			if(i == MwGetInteger(handle, MwNvalue) && n == 1) {
				r.y += h;
				r.height -= h;
				MwDrawFrame(handle, &r, c, 0);

				render = 1;
			} else if(n == 0) {
				render = 1;
			}

			r2.x	  = x;
			r2.y	  = 0;
			r2.width  = tab_width(handle, t->names[i]) + iw;
			r2.height = tab_height(handle) + MwDefaultBorderWidth(handle);

			x += r2.width;

			if(iw > 0) {
				MwLLPixmap px = MwGetVoid(t->frames[i], MwNiconPixmap);
				MwRect	   rp;

				rp = r2;

				rp.x += 5;
				rp.y += 5;

				rp.width  = iw - 10;
				rp.height = rp.height * (iw - 10) / iw;

				MwLLDrawPixmap(handle->lowlevel, &rp, px);
			}

			if(render) {
				MwPoint p;

				if(n == 1 && i > 0) {
					MwRect r3 = r2;

					r3.y	 = tab_height(handle);
					r3.width = r3.height = MwDefaultBorderWidth(handle);

					MwLLClip(handle->lowlevel, &r3);

					r3.x -= r3.width;
					r3.y -= r3.height;
					r3.width *= 2;
					r3.height *= 2;
					MwDrawFrame(handle, &r3, c, 1);

					MwLLClip(handle->lowlevel, NULL);
				} else if(n == 1 && i == 0) {
					MwRect r3 = r2;

					r3.y	 = tab_height(handle);
					r3.width = r3.height = MwDefaultBorderWidth(handle);

					MwLLClip(handle->lowlevel, &r3);

					r3.x -= r3.width;
					r3.y -= r3.height;
					r3.width *= 2;
					r3.height *= 3;
					MwDrawFrame(handle, &r3, c, 1);

					MwLLClip(handle->lowlevel, NULL);
				}

				if(n == 1 && i < (arrlen(t->names) - 1)) {
					MwRect r3 = r2;

					r3.x += r2.width - MwDefaultBorderWidth(handle);
					r3.y	 = tab_height(handle);
					r3.width = r3.height = MwDefaultBorderWidth(handle);

					MwLLClip(handle->lowlevel, &r3);

					r3.y -= r3.height;
					r3.width *= 2;
					r3.height *= 2;
					MwDrawFrame(handle, &r3, c, 1);

					MwLLClip(handle->lowlevel, NULL);
				}

				if(n == 1) {
					MwRect r3 = r2;

					r3.height -= MwDefaultBorderWidth(handle);

					MwLLClip(handle->lowlevel, &r3);
				}

				MwDrawFrame(handle, &r2, c, 0);

				MwLLClip(handle->lowlevel, NULL);

				if(n == 1) {
					MwRect r3 = r2;

					r3.y += r3.height;
					r3.height = MwDefaultBorderWidth(handle);

					MwDrawRect(handle, &r3, c);
				}

				p.x = r2.x + (r2.width + iw / 2) / 2;
				p.y = r2.y + r2.height / 2;
				MwDrawText(handle, NULL, &p, t->names[i], MwALIGNMENT_CENTER, ct);
			}
		}
	}

	MwLLFreeColor(ct);
	MwLLFreeColor(c);
}

static void show_frame(MwWidget handle) {
	resize(handle);

	MwForceRender(handle);

	MwDispatchUserHandler(handle, MwNchangedHandler, NULL);
}

static void click(MwWidget handle) {
	MwTab t = handle->internal;
	int   i;
	int   x = 0;
	int   h = tab_height(handle);

	for(i = 0; i < arrlen(t->names); i++) {
		int w  = tab_width(handle, t->names[i]);
		int iw = icon_width(handle, t->names[i]);

		w += iw;

		if(MwGetInteger(handle, MwNvalue) != i && x <= handle->mouse_point.x && handle->mouse_point.x <= (x + w) && 0 <= handle->mouse_point.y && handle->mouse_point.y <= h) {
			MwSetInteger(handle, MwNvalue, i);
			show_frame(handle);
		}

		x += w;
	}
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNvalue) == 0) {
		show_frame(handle);
	}
}

static MwWidget mwTabAddImpl(MwWidget handle, const char* name) {
	MwTab	 t = handle->internal;
	MwWidget f = MwCreateWidget(MwFrameClass, "tabframe", handle, MwDefaultBorderWidth(handle), tab_height(handle) + MwDefaultBorderWidth(handle), MwGetInteger(handle, MwNwidth) - MwDefaultBorderWidth(handle) * 2, MwGetInteger(handle, MwNheight) - tab_height(handle) - MwDefaultBorderWidth(handle) * 2);
	char*	 n = MwStringDuplicate(name);

	arrput(t->frames, f);
	arrput(t->names, n);

	MwForceRender(handle);

	if(MwGetInteger(handle, MwNvalue) == -1) {
		MwSetInteger(handle, MwNvalue, arrlen(t->frames) - 1);
		show_frame(handle);
	} else {
		MwLLShow(f->lowlevel, 0);
	}

	return f;
}

static MwWidget mwTabGetFrameImpl(MwWidget handle, const char* name) {
	MwTab t = handle->internal;
	int   i;

	for(i = 0; i < arrlen(t->names); i++) {
		if(strcmp(t->names[i], name) == 0) return t->frames[i];
	}

	return NULL;
}

static void mwTabFocusImpl(MwWidget handle, const char* name) {
	MwTab t = handle->internal;
	int   i;

	for(i = 0; i < arrlen(t->names); i++) {
		if(strcmp(t->names[i], name) == 0) {
			MwSetInteger(handle, MwNvalue, i);
			show_frame(handle);
			break;
		}
	}
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	if(strcmp(name, "mwTabAdd") == 0) {
		const char* name = va_arg(va, const char*);

		*(MwWidget*)out = mwTabAddImpl(handle, name);
	} else if(strcmp(name, "mwTabGetFrame") == 0) {
		const char* name = va_arg(va, const char*);

		*(MwWidget*)out = mwTabGetFrameImpl(handle, name);
	} else if(strcmp(name, "mwTabFocus") == 0) {
		const char* name = va_arg(va, const char*);

		mwTabFocusImpl(handle, name);
	}
}

static void resize(MwWidget handle) {
	MwTab t = handle->internal;
	int   v = MwGetInteger(handle, MwNvalue);
	int   i;

	for(i = 0; i < arrlen(t->frames); i++) {
		int y = MwGetInteger(handle, MwNheight);

		MwLLShow(t->frames[i]->lowlevel, i == v);
		if(i == v) y = tab_height(handle) + MwDefaultBorderWidth(handle);

		MwVaApply(t->frames[i],
			  MwNx, MwDefaultBorderWidth(handle),
			  MwNy, y,
			  MwNwidth, MwGetInteger(handle, MwNwidth) - MwDefaultBorderWidth(handle) * 2,
			  MwNheight, MwGetInteger(handle, MwNheight) - tab_height(handle) - MwDefaultBorderWidth(handle) * 2,
			  NULL);
	}
}

static void children_update(MwWidget handle, MwWidget child, int new_child) {
	MwTab t = handle->internal;
	int   v = MwGetInteger(handle, MwNvalue);
	int   n;
	int   i;

	if(new_child) return;

	for(i = 0; i < arrlen(t->frames); i++) {
		if(t->frames[i] == child) {
			n = i;
			break;
		}
	}

	free(t->names[n]);

	arrdel(t->frames, n);
	arrdel(t->names, n);

	if(n <= v) {
		if(v > 0) v--;

		MwSetInteger(handle, MwNvalue, v);
		show_frame(handle);
	} else {
		MwSetInteger(handle, MwNvalue, v);
		show_frame(handle);
	}

	MwForceRender(handle);
}

static void children_prop_change(MwWidget handle, MwWidget child, const char* key) {
	(void)child;

	if(strcmp(key, MwNiconPixmap) == 0) {
		MwForceRender(handle);
	}
}

MwClassRec MwTabClassRec = {
    wcreate,		  /* create */
    destroy,		  /* destroy */
    draw,		  /* draw */
    click,		  /* click */
    NULL,		  /* parent_resize */
    prop_change,	  /* prop_change */
    NULL,		  /* mouse_move */
    NULL,		  /* mouse_up */
    NULL,		  /* mouse_down */
    NULL,		  /* key */
    func_handler,	  /* execute */
    NULL,		  /* tick */
    resize,		  /* resize */
    children_update,	  /* children_update */
    children_prop_change, /* children_prop_change */
    NULL,		  /* clipboard */
    NULL,		  /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwTabClass = &MwTabClassRec;
