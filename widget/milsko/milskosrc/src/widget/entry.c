#include <Mw/Milsko.h>
#include "../../external/stb_ds.h"

static int wcreate(MwWidget handle) {
	MwEntry	 t		= malloc(sizeof(*t));
	MwWidget topmost_parent = handle->parent;

	t->cursor	      = 0;
	t->right	      = 0;
	t->length	      = 0;
	t->cursor_blink_timer = 0;
	handle->internal      = t;

	while(topmost_parent->parent) topmost_parent = topmost_parent->parent;
	arrpush(topmost_parent->monitored_entries, handle);

	MwSetDefault(handle);

	MwLLSetCursor(handle->lowlevel, &MwCursorText, &MwCursorTextMask);

	return 0;
}

static void destroy(MwWidget handle) {
	int	 i;
	MwWidget topmost_parent = handle->parent;

	while(topmost_parent->parent) topmost_parent = topmost_parent->parent;
	for(i = 0; i < arrlen(topmost_parent->monitored_entries); i++) {
		if(topmost_parent->monitored_entries[i] == handle) {
			arrdel(topmost_parent->monitored_entries, i);
			break;
		}
	}
	free(handle->internal);
}
static void tick(MwWidget handle) {
	MwEntry t = handle->internal;
	if(t->active) {
		if(++t->cursor_blink_timer >= 30) {
			t->cursor_blink_timer = 0;
		}
		MwDispatch(handle, draw);
	}
}
static void draw(MwWidget handle) {
	MwRect	    r;
	MwEntry	    t	 = handle->internal;
	MwLLColor   base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor   text = MwParseColor(handle, MwGetText(handle, MwNforeground));
	const char* str	 = MwGetText(handle, MwNtext);
	MwLLPixmap  bgpx = MwGetVoid(handle, MwNbackgroundPixmap);
	MwFLFont    font = MwFLBuildFont(MwFLFlagMonospace);
	if(str == NULL) str = "";

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth) - t->right;
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawWidgetBack(handle, &r, base, 1, MwDEFAULT);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);
	if(str != NULL) {
		int	w = MwTextWidth(handle, font, "M");
		int	h = MwTextHeight(handle, font, "M");
		MwPoint p;
		char*	show;
		int	len;
		int	i;
		int	start;
		int	textlen;
		int	attr;
		MwRect	currc;

		p.x = (r.width - (r.width / w * w)) / 2;
		p.y = r.height / 2;

		len = (r.width - p.x * 2) / w;

		if(len > 0) {
			show = malloc(len * 4 + 1);
			memset(show, 0, len * 4 + 1);

			start = (t->cursor - 1) / len;
			start *= len;
			if((attr = MwGetInteger(handle, MwNhideInput)) == MwDEFAULT || !attr) {
				MwUTF8Copy(str, start, show, 0, len);
			} else {
				for(i = 0; i < MwUTF8Length(str) - start; i++) show[i] = '*';
			}

			MwDrawText(handle, font, &p, show, MwALIGNMENT_BEGINNING, text);

			textlen = (t->cursor - 1) % len + 1;
			for(i = 0; i < textlen; i++) show[i] = 'M';
			show[i] = 0;

			if(t->active && t->cursor_blink_timer <= 15) {
				currc.x	     = p.x + MwTextWidth(handle, font, show);
				currc.y	     = (r.height - h) / 2 + MwDefaultBorderWidth(handle);
				currc.width  = 1;
				currc.height = h;
				MwDrawRect(handle, &currc, text);
			}

			free(show);
		}
	}

	MwLLFreeColor(text);
	MwLLFreeColor(base);
}

static void key(MwWidget handle, int code) {
	MwEntry	    t	= handle->internal;
	const char* str = MwGetText(handle, MwNtext);
	char*	    out;
	if(str == NULL) str = "";

	if(code == MwKEY_BACKSPACE) {
		if(t->cursor == 0) return;
		out = malloc(strlen(str) + 1);

		t->cursor--;

		MwUTF8Copy(str, 0, out, 0, t->cursor);
		MwUTF8Copy(str, t->cursor + 1, out, t->cursor, MwUTF8Length(str) - (t->cursor + 1));

		MwSetText(handle, MwNtext, out);
		MwDispatchUserHandler(handle, MwNchangedHandler, NULL);

		free(out);
	} else if(code == MwKEY_LEFT) {
		if(t->cursor == 0) return;
		t->cursor--;
	} else if(code == MwKEY_RIGHT) {
		if(t->cursor == MwUTF8Length(str)) return;
		t->cursor++;
	} else if(code == MwKEY_ENTER) {
		MwDispatchUserHandler(handle, MwNactivateHandler, NULL);
	} else if(code == (MwKEY_CONTROL_FLAG | 'v')) {
		MwLLGetClipboard(handle->lowlevel, MwCLIPBOARD_MAIN);
	} else if(!(code & MwKEY_FLAG)) {
		int incr = 0;
		out	 = malloc(strlen(str) + 5 + 1);
		incr += MwUTF8Copy(str, 0, out, 0, t->cursor);
		MwUTF32ToUTF8(code, out + incr);
		MwUTF8Copy(str, t->cursor, out, t->cursor + 1, MwUTF8Length(str) - t->cursor);

		t->cursor++;

		MwSetText(handle, MwNtext, out);
		MwDispatchUserHandler(handle, MwNchangedHandler, NULL);

		free(out);
	}

	if(MwGetText(handle, MwNtext) != NULL) t->length = MwUTF8Length(MwGetText(handle, MwNtext));

	MwForceRender(handle);
}

#if defined(USE_X11) || defined(USE_WAYLAND)
static void mouse_up(MwWidget handle, void* ptr) {
	if(handle->lowlevel->common.type == MwLLBackendWayland || handle->lowlevel->common.type == MwLLBackendX11) {
		MwMouse* mouse = ptr;
		if(mouse->button == MwMOUSE_MIDDLE) {
			MwLLGetClipboard(handle->lowlevel, MwCLIPBOARD_PRIMARY);
		}
	}
	MwForceRender2(handle, NULL);
}
#else
#define mouse_up MwForceRender2
#endif

static void mouse_down(MwWidget handle, void* ptr) {
	int	 i		= 0;
	MwEntry	 t		= handle->internal;
	MwWidget topmost_parent = handle->parent;

	while(topmost_parent->parent) topmost_parent = topmost_parent->parent;

	for(i = 0; i < arrlen(topmost_parent->monitored_entries); i++) {
		MwEntry t	      = topmost_parent->monitored_entries[i]->internal;
		t->active	      = MwFALSE;
		t->cursor_blink_timer = 0;
		MwDispatch(topmost_parent->monitored_entries[i], draw);
	}
	t->active = MwTRUE;
	MwForceRender2(handle, NULL);
}

static void prop_change(MwWidget handle, const char* prop) {
	if(strcmp(prop, MwNtext) == 0 || strcmp(prop, MwNhideInput) == 0) MwForceRender(handle);

	if(strcmp(prop, MwNtext) == 0) {
		MwEntry t   = handle->internal;
		int	len = MwUTF8Length(MwGetText(handle, MwNtext));
		if(len < t->cursor) {
			t->cursor = len;
		} else if(t->length == t->cursor) {
			t->cursor = len;
		}
		t->length = len;
	}
}

static void clipboard(MwWidget handle, const char* data) {
	MwEntry	    t	= handle->internal;
	const char* str = MwGetText(handle, MwNtext);
	char*	    out;
	if(!str) {
		str = "";
	}
	out = malloc(strlen(str) + strlen(data) + 1);

	if(str == NULL) str = "";

	MwUTF8Copy(str, 0, out, 0, t->cursor);
	MwUTF8Copy(data, 0, out, t->cursor, MwUTF8Length(data));
	MwUTF8Copy(str, t->cursor, out, t->cursor + MwUTF8Length(data), MwUTF8Length(str) - t->cursor);

	t->cursor += MwUTF8Length(data);

	MwSetText(handle, MwNtext, out);
	free(out);

	MwForceRender(handle);
}

MwClassRec MwEntryClassRec = {
    wcreate,	 /* create */
    destroy,	 /* destroy */
    draw,	 /* draw */
    NULL,	 /* click */
    NULL,	 /* parent_resize */
    prop_change, /* prop_change */
    NULL,	 /* mouse_move */
    mouse_up,	 /* mouse_up */
    mouse_down,	 /* mouse_down */
    key,	 /* key */
    NULL,	 /* execute */
    tick,	 /* tick */
    NULL,	 /* resize */
    NULL,	 /* children_update */
    NULL,	 /* children_prop_change */
    clipboard,	 /* clipboard */
    NULL,	 /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwEntryClass = &MwEntryClassRec;
