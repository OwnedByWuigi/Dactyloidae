#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

#define OpenerSize 11
#define LineSpace 16
#define LinePadding 4

#define Font MwFLBuildFont(MwFLFlagMonospace)

static void MWAPI vscroll_changed(MwWidget handle, void* user, void* call) {
	MwTreeView tv = handle->parent->internal;

	(void)user;
	(void)call;

	tv->changed = 1;
}

static void set_all(MwTreeViewEntry** root, int v) {
	int i;
	for(i = 0; i < arrlen(root); i++) {
		root[i]->selected = v;
		set_all(root[i]->tree, v);
	}
}

static void recursion(MwWidget handle, MwTreeViewEntry* tree, MwTreeViewEntry** root, MwLLColor base, MwLLColor text, MwLLColor base2, MwLLColor text2, MwPoint* p, int next, int shift, int* skip, int* shared, int draw, MwPoint* mouse) {
	int	i;
	MwPoint l[2];
	int	skipped = 0;
	if((*skip) > 0) {
		(*skip)--;
		skipped = 1;
		if(p->y == MwDefaultBorderWidth(handle)) p->y -= MwTextHeight(handle, Font, "M");
	} else if((*shared) < (MwGetInteger(handle, MwNheight) / MwTextHeight(handle, Font, "M") + 2)) {
		MwRect r;
		p->x += shift;
		p->y += MwTextHeight(handle, Font, "M") / 2 + LinePadding;

		if(tree->tree != NULL || shift > LineSpace) {
			l[0] = *p;
			l[0].x -= LineSpace / 2;
			l[1] = *p;
			if(draw) MwLLLine(handle->lowlevel, &l[0], text2);
		}

		if(shift > LineSpace) {
			l[0] = *p;
			l[0].x -= LineSpace / 2;
			l[1] = l[0];
			l[0].y -= MwTextHeight(handle, Font, "M") / 2 + LinePadding;
			if(next) {
				l[1].y += MwTextHeight(handle, Font, "M") / 2;
			}
			if(draw) MwLLLine(handle->lowlevel, &l[0], text2);
		}
		if(tree->tree != NULL) {
			r.width	 = OpenerSize;
			r.height = OpenerSize;
			r.x	 = p->x - LineSpace + (LineSpace - r.width) / 2;
			r.y	 = p->y - MwTextHeight(handle, Font, "M") / 2 + (MwTextHeight(handle, Font, "M") - r.height) / 2;
			if(draw) {
				MwLLColor col = tree->opened ? MwLightenColor(handle, base, -8, -8, -8) : base;
				MwRect	  r2  = r;
				const int len = 4;

				MwDrawRect(handle, &r, col);
				MwDrawFrame(handle, &r, base, tree->opened);
				if(tree->opened) {
					l[0].x = r.x + (r.width - 1 - len) / 2;
					l[0].y = r.y + r.height / 2;

					l[1].x = l[0].x + len;
					l[1].y = l[0].y;

					MwLLLine(handle->lowlevel, &l[0], text);
				} else {
					l[0].x = r.x + (r.width - len) / 2;
					l[0].y = r.y + r.height / 2;

					l[1].x = l[0].x + len;
					l[1].y = l[0].y;

					MwLLLine(handle->lowlevel, &l[0], text);

					l[0].x = r.x + r.width / 2;
					l[0].y = r.y + (r.height - len) / 2;

					l[1].x = l[0].x;
					l[1].y = l[0].y + len;

					MwLLLine(handle->lowlevel, &l[0], text);
				}
				r = r2;
				MwDrawFrame(handle, &r, base, tree->opened);

				if(col != base) MwLLFreeColor(col);
			} else {
				if(r.x <= mouse->x && mouse->x <= (r.x + r.width) && r.y <= mouse->y && mouse->y <= (r.y + r.height)) {
					tree->opened = tree->opened ? 0 : 1;
				}
			}
		}
		if(tree->pixmap != NULL) {
			r.height = MwTextHeight(handle, Font, "M");
			r.width	 = r.height * tree->pixmap->common.width / tree->pixmap->common.height;

			r.x = p->x + (MwGetInteger(handle->parent, MwNleftPadding) - r.width) / 2;
			r.y = p->y - MwTextHeight(handle, Font, "M") / 2;

			if(draw) MwLLDrawPixmap(handle->lowlevel, &r, tree->pixmap);
		}
		p->x += MwGetInteger(handle->parent, MwNleftPadding);
		if(draw) {
			if(tree->selected) {
				r.x	 = p->x;
				r.y	 = p->y - MwTextHeight(handle, Font, "M") / 2;
				r.width	 = MwGetInteger(handle, MwNwidth) - MwGetInteger(handle->parent, MwNleftPadding) - shift;
				r.height = MwTextHeight(handle, Font, "M");
				MwDrawRect(handle, &r, text2);
			}
			handle->bgcolor = tree->selected ? text2 : base2;
			MwDrawText(handle, Font, p, tree->label, MwALIGNMENT_BEGINNING, tree->selected ? base2 : text2);
			handle->bgcolor = NULL;
		} else {
			if(p->x <= mouse->x && mouse->x <= (p->x + MwTextWidth(handle, Font, tree->label)) && (p->y - MwTextHeight(handle, Font, "M") / 2) <= mouse->y && mouse->y <= (p->y + MwTextHeight(handle, Font, "M") / 2)) {
				unsigned long t;

				set_all(root, 0);
				tree->selected = 1;
				if(((t = MwTimeGetTick()) - tree->click_time) < MwDoubleClickTimeout || MwGetInteger(handle->parent, MwNsingleClickSelectable)) {
					MwDispatchUserHandler(handle->parent, MwNtreeViewActivateHandler, tree);
				}

				tree->click_time = t;
			}
		}
		p->x -= MwGetInteger(handle->parent, MwNleftPadding);

		p->y += MwTextHeight(handle, Font, "M") / 2;
		p->x -= shift;

		(*shared)++;
	}
	if(!tree->opened) return;
	for(i = 0; i < arrlen(tree->tree); i++) {
		l[0] = *p;
		l[0].x += shift + LineSpace / 2;
		l[0].y += MwTextHeight(handle, Font, "M") - (MwTextHeight(handle, Font, "M") - OpenerSize) / 2;
		if(tree->tree[i]->tree != NULL) {
			l[0].y += OpenerSize / 2;
		}

		recursion(handle, tree->tree[i], root, base, text, base2, text2, p, i != (arrlen(tree->tree) - 1) ? 1 : 0, shift + LineSpace, skip, shared, draw, mouse);

		l[1] = *p;
		l[1].x += shift + LineSpace / 2;

		if(skipped && p->y > l[0].y) {
			skipped = 0;
		}
		if(draw && !skipped && i != (arrlen(tree->tree) - 1)) MwLLLine(handle->lowlevel, &l[0], text);
	}
}

static void frame_draw(MwWidget handle) {
	MwRect	   r, r2;
	MwTreeView tv	 = handle->parent->internal;
	MwLLColor  base	 = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor  base2 = MwParseColor(handle, MwGetText(handle, MwNsubBackground));
	MwLLColor  text	 = MwParseColor(handle, MwGetText(handle, MwNforeground));
	MwLLColor  text2 = MwParseColor(handle, MwGetText(handle, MwNsubForeground));
	MwPoint	   p;
	int	   shared = 0;
	int	   i;
	int	   skip = MwGetInteger(tv->vscroll, MwNvalue) * (MwGetInteger(tv->vscroll, MwNmaxValue) - MwGetInteger(tv->vscroll, MwNareaShown)) / MwGetInteger(tv->vscroll, MwNmaxValue);

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);
	r2	 = r;

	p.x = MwDefaultBorderWidth(handle);
	p.y = MwDefaultBorderWidth(handle);

	MwDrawFrame(handle, &r, base, 1);
	MwDrawRect(handle, &r, base2);

	r = r2;

	for(i = 0; i < arrlen(tv->tree); i++) {
		if(shared > (r.height / MwTextHeight(handle, Font, "M"))) break;
		recursion(handle, tv->tree[i], tv->tree, base, text, base2, text2, &p, 0, LineSpace, &skip, &shared, 1, NULL);
	}

	MwDrawFrame(handle, &r, base, 1);

	MwLLFreeColor(text2);
	MwLLFreeColor(text);
	MwLLFreeColor(base2);
	MwLLFreeColor(base);
}

static int recursive_length(MwTreeViewEntry** e) {
	int l = 0;
	int i;
	for(i = 0; i < arrlen(e); i++) {
		if(e[i]->opened && e[i]->tree != NULL) l += recursive_length(e[i]->tree);
		l++;
	}
	return l;
}

static void MWAPI frame_mouse_down(MwWidget handle, void* user, void* call) {
	MwTreeView tv	 = handle->parent->internal;
	MwMouse*   mouse = call;

	(void)user;

	if(mouse->button == MwMOUSE_LEFT) tv->pressed = mouse->point;
}

static void resize(MwWidget handle, int no_resize);

static void MWAPI frame_mouse_up(MwWidget handle, void* user, void* call) {
	MwTreeView tv	  = handle->parent->internal;
	int	   shared = 0;
	int	   skip	  = MwGetInteger(tv->vscroll, MwNvalue) * (MwGetInteger(tv->vscroll, MwNmaxValue) - MwGetInteger(tv->vscroll, MwNareaShown)) / MwGetInteger(tv->vscroll, MwNmaxValue);
	MwPoint	   p;
	MwMouse*   mouse = call;

	(void)user;

	if(mouse->button == MwMOUSE_LEFT) {
		int i;
		p.x = MwDefaultBorderWidth(tv->frame);
		p.y = MwDefaultBorderWidth(tv->frame);
		for(i = 0; i < arrlen(tv->tree); i++) {
			if(shared > (MwGetInteger(tv->frame, MwNheight) / MwTextHeight(tv->frame, Font, "M"))) break;
			recursion(tv->frame, tv->tree[i], tv->tree, NULL, NULL, NULL, NULL, &p, 0, LineSpace, &skip, &shared, 0, &tv->pressed);
		}
		resize(handle->parent, 1);
		MwForceRender(tv->frame);
	}
}

static void resize(MwWidget handle, int no_resize) {
	MwTreeView tv = handle->internal;
	int	   w  = MwGetInteger(handle, MwNwidth);
	int	   h  = MwGetInteger(handle, MwNheight);
	int	   ih = 0;
	int	   m  = 0;

	if(tv->vscroll == NULL) {
		tv->vscroll = MwCreateWidget(MwScrollBarClass, "vscroll", handle, w - 16, 0, 16, h);
		MwAddUserHandler(tv->vscroll, MwNchangedHandler, vscroll_changed, NULL);
		MwSetInteger(tv->vscroll, MwNvalue, 5);
	} else if(!no_resize) {
		MwVaApply(tv->vscroll,
			  MwNx, w - 16,
			  MwNy, 0,
			  MwNwidth, 16,
			  MwNheight, h,
			  NULL);
	}

	ih = recursive_length(tv->tree);
	if(ih == 0) ih = 1;

	if(ih <= (h / (LinePadding + MwTextHeight(handle, Font, "M")))) {
		MwLLShow(tv->vscroll->lowlevel, 0);
	} else {
		MwLLShow(tv->vscroll->lowlevel, 1);
		m = 16;
	}

	if(tv->frame == NULL) {
		tv->frame	       = MwVaCreateWidget(MwFrameClass, "frame", handle, 0, 0, w - m, h,
							  MwNhasBorder, 1,
							  MwNinverted, 1,
							  NULL);
		tv->frame->draw_inject = frame_draw;
		MwAddUserHandler(tv->frame, MwNmouseDownHandler, frame_mouse_down, NULL);
		MwAddUserHandler(tv->frame, MwNmouseUpHandler, frame_mouse_up, NULL);
	} else if(!no_resize || MwGetInteger(tv->frame, MwNwidth) != (w - m)) {
		MwVaApply(tv->frame,
			  MwNx, 0,
			  MwNy, 0,
			  MwNwidth, w - m,
			  MwNheight, h,
			  NULL);
	}

	MwVaApply(tv->vscroll,
		  MwNareaShown, h / (LinePadding + MwTextHeight(handle, Font, "M")),
		  MwNmaxValue, ih,
		  NULL);
}

static int wcreate(MwWidget handle) {
	MwTreeView tv = malloc(sizeof(*tv));
	memset(tv, 0, sizeof(*tv));

	handle->internal = tv;

	MwSetDefault(handle);

	MwSetInteger(handle, MwNsingleClickSelectable, 0);
	MwSetInteger(handle, MwNleftPadding, 0);

	resize(handle, 0);
	tv->changed = 0;

	return 0;
}

static void destroy(MwWidget handle) {
	free(handle->internal);
}

static void draw(MwWidget handle) {
	MwLLColor c = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwRect	  r;

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);
	MwDrawRect(handle, &r, c);

	MwLLFreeColor(c);
}

static void* mwTreeViewAddImpl(MwWidget handle, void* parent, MwLLPixmap pixmap, const char* item) {
	MwTreeView	 tv = handle->internal;
	MwTreeViewEntry* t  = malloc(sizeof(*t));

	t->label      = MwStringDuplicate(item);
	t->pixmap     = pixmap;
	t->selected   = 0;
	t->opened     = 1;
	t->click_time = 0;
	t->tree	      = NULL;
	t->parent     = parent;

	if(parent == NULL) {
		arrput(tv->tree, t);
	} else {
		MwTreeViewEntry* e = parent;

		arrput(e->tree, t);
	}
	resize(handle, 1);
	MwForceRender(tv->frame);

	return t;
}

static void free_all(MwTreeViewEntry** tree) {
	int i;
	for(i = 0; i < arrlen(tree); i++) {
		free(tree[i]->label);
		free_all(tree[i]->tree);
		free(tree[i]);
	}
	arrfree(tree);
}

static void mwTreeViewDeleteImpl(MwWidget handle, void* item) {
	MwTreeView	 tv = handle->internal;
	MwTreeViewEntry* e  = item;
	MwTreeViewEntry* p  = e->parent;

	(void)handle;

	free_all(p->tree);
	p->tree = NULL;
	resize(handle, 1);
	MwForceRender(tv->frame);
}

static void mwTreeViewResetImpl(MwWidget handle) {
	MwTreeView tv = handle->internal;

	free_all(tv->tree);
	tv->tree = NULL;
	resize(handle, 1);
	MwForceRender(tv->frame);
}

static const char* mwTreeViewGetImpl(MwWidget handle, void* item) {
	MwTreeViewEntry* e = item;

	(void)handle;

	return e->label;
}

static void mwTreeViewSetLabelImpl(MwWidget handle, void* item, const char* label) {
	MwTreeView	 tv = handle->internal;
	MwTreeViewEntry* e  = item;

	free(e->label);
	e->label = MwStringDuplicate(label);

	if(e->parent == NULL || (e->parent != NULL && e->parent->opened)) {
		resize(handle, 1);
		MwForceRender(tv->frame);
	}
}

static void mwTreeViewSetPixmapImpl(MwWidget handle, void* item, MwLLPixmap pixmap) {
	MwTreeView	 tv = handle->internal;
	MwTreeViewEntry* e  = item;

	e->pixmap = pixmap;

	if(e->parent == NULL || (e->parent != NULL && e->parent->opened)) {
		resize(handle, 1);
		MwForceRender(tv->frame);
	}
}

static void mwTreeViewSetOpenedImpl(MwWidget handle, void* item, int opened) {
	MwTreeView	 tv = handle->internal;
	MwTreeViewEntry* e  = item;

	e->opened = opened;

	if(e->parent == NULL || (e->parent != NULL && e->parent->opened)) {
		resize(handle, 1);
		MwForceRender(tv->frame);
	}
}

static int mwTreeViewGetOpenedImpl(MwWidget handle, void* item) {
	MwTreeViewEntry* e = item;

	(void)handle;

	return e->opened;
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	if(strcmp(name, "mwTreeViewAdd") == 0) {
		void*	    parent = va_arg(va, void*);
		MwLLPixmap  pixmap = va_arg(va, MwLLPixmap);
		const char* item   = va_arg(va, const char*);
		*(void**)out	   = mwTreeViewAddImpl(handle, parent, pixmap, item);
	}
	if(strcmp(name, "mwTreeViewDelete") == 0) {
		void* item = va_arg(va, void*);
		mwTreeViewDeleteImpl(handle, item);
	}
	if(strcmp(name, "mwTreeViewReset") == 0) {
		mwTreeViewResetImpl(handle);
	}
	if(strcmp(name, "mwTreeViewGet") == 0) {
		void* item	   = va_arg(va, void*);
		*(const char**)out = mwTreeViewGetImpl(handle, item);
	}
	if(strcmp(name, "mwTreeViewSetLabel") == 0) {
		void*	    item  = va_arg(va, void*);
		const char* label = va_arg(va, const char*);
		mwTreeViewSetLabelImpl(handle, item, label);
	}
	if(strcmp(name, "mwTreeViewSetPixmap") == 0) {
		void*	   item	  = va_arg(va, void*);
		MwLLPixmap pixmap = va_arg(va, MwLLPixmap);
		mwTreeViewSetPixmapImpl(handle, item, pixmap);
	}
	if(strcmp(name, "mwTreeViewSetOpened") == 0) {
		void* item   = va_arg(va, void*);
		int   opened = va_arg(va, int);
		mwTreeViewSetOpenedImpl(handle, item, opened);
	}
	if(strcmp(name, "mwTreeViewGetOpened") == 0) {
		void* item = va_arg(va, void*);
		*(int*)out = mwTreeViewGetOpenedImpl(handle, item);
	}
}

static void tick(MwWidget handle) {
	MwTreeView tv = handle->internal;

	if(tv->changed) {
		tv->changed = 0;
		MwForceRender(tv->frame);
	}
}

static void props_change(MwWidget handle, char** props) {
	int i;
	int rsz = 0;

	for(i = 0; props[i] != NULL; i++) {
		if(strcmp(props[i], MwNwidth) == 0 || strcmp(props[i], MwNheight) == 0) rsz = 1;
	}

	if(rsz) resize(handle, 0);
}

MwClassRec MwTreeViewClassRec = {
    wcreate,	  /* create */
    destroy,	  /* destroy */
    draw,	  /* draw */
    NULL,	  /* click */
    NULL,	  /* parent_resize */
    NULL,	  /* prop_change */
    NULL,	  /* mouse_move */
    NULL,	  /* mouse_up */
    NULL,	  /* mouse_down */
    NULL,	  /* key */
    func_handler, /* execute */
    tick,	  /* tick */
    NULL,	  /* resize */
    NULL,	  /* children_update */
    NULL,	  /* children_prop_change */
    NULL,	  /* clipboard */
    props_change, /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwTreeViewClass = &MwTreeViewClassRec;
