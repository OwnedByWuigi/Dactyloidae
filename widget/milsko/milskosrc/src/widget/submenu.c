#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

static int wcreate(MwWidget handle) {
	MwLLBeginStateChange(handle->lowlevel);

	MwSetDefault(handle);
	MwSetInteger(handle, MwNleftPadding, 0);

	handle->internal = NULL;

	return 0;
}

static void null_all(MwMenu menu) {
	int i;
	for(i = 0; i < arrlen(menu->sub); i++) {
		null_all(menu->sub[i]);
	}
	menu->wsub = NULL;
}

static void destroy(MwWidget handle) {
	MwMenu menu = handle->internal;

	menu->wsub = NULL;
	null_all(menu);
}

static void draw(MwWidget handle) {
	MwLLColor base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor text = MwParseColor(handle, MwGetText(handle, MwNforeground));
	MwRect	  r;
	MwMenu	  menu = handle->internal;

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawWidgetBack(handle, &r, base, 0, MwDEFAULT);

	if(menu != NULL) {
		MwPoint p;
		int	i;

		p.x = 0;
		p.y = 3;

		for(i = 0; i < arrlen(menu->sub); i++) {
			if(strcmp(menu->sub[i]->name, "----") == 0) {
				MwRect rc;

				p.y += 1;

				rc.x	  = MwDefaultBorderWidth(handle) * 2;
				rc.y	  = p.y;
				rc.width  = r.width - (rc.x * 2) - MwGetInteger(handle, MwNleftPadding);
				rc.height = 2;

				rc.x += MwGetInteger(handle, MwNleftPadding);

				MwDrawFrameEx(handle, &rc, base, 1, 1, 0, 0);

				p.y += 2 + 1;
			} else {
				int tw = MwTextWidth(handle, NULL, menu->sub[i]->name);
				int th = MwTextHeight(handle, NULL, menu->sub[i]->name);

				if(menu->sub[i]->wsub != NULL) {
					r.x	 = MwGetInteger(handle, MwNleftPadding);
					r.y	 = p.y - 3;
					r.width	 = MwGetInteger(handle, MwNwidth) - MwGetInteger(handle, MwNleftPadding);
					r.height = th + 3 * 2;
					MwDrawWidgetBack(handle, &r, base, 0, MwDEFAULT);
				}

				p.x = 5 + tw / 2 + MwGetInteger(handle, MwNleftPadding);

				p.y += th / 2;
				MwDrawText(handle, MwFLBuildFont(menu->sub[i]->wsub != NULL ? MwFLFlagBold : 0), &p, menu->sub[i]->name, MwALIGNMENT_CENTER, text);

				if(arrlen(menu->sub[i]->sub) > 0) {
					MwRect tr;

					tr.x	 = MwGetInteger(handle, MwNwidth) - MwDefaultBorderWidth(handle) - 11 - 2;
					tr.y	 = p.y - th / 2 + 2;
					tr.width = tr.height = 11;

					MwDrawTriangle(handle, &tr, base, menu->sub[i]->wsub != NULL ? 1 : 0, MwEAST);
				}

				p.y += th / 2 + 3;
			}
		}
	}

	MwLLFreeColor(text);
	MwLLFreeColor(base);
}

static void click(MwWidget handle) {
	MwWidget w    = handle;
	MwMenu	 menu = handle->internal;

	if(arrlen(menu->sub) > 0) {
		int    ww = 0, i;
		MwRect rc;

		rc.x = 5;
		rc.y = 3;
		for(i = 0; i < arrlen(menu->sub); i++) {
			int tw = MwTextWidth(handle, NULL, menu->sub[i]->name);
			if(tw > ww) ww = tw;
		}

		rc.width = ww + 15;
		for(i = 0; i < arrlen(menu->sub); i++) {
			int th	  = MwTextHeight(handle, NULL, menu->sub[i]->name);
			rc.height = th;

			if(strcmp(menu->sub[i]->name, "----") == 0) {
				rc.height = 2 - 1;
			}

			if(MwGetInteger(handle, MwNleftPadding) <= handle->mouse_point.x && rc.y <= handle->mouse_point.y && handle->mouse_point.y <= (int)(rc.y + rc.height)) {
				if(menu->sub[i]->wsub == NULL && arrlen(menu->sub[i]->sub) > 0) {
					MwPoint p;
					int	j;

					for(j = 0; j < arrlen(menu->sub); j++) {
						if(menu->sub[j]->wsub != NULL) MwDestroyWidget(menu->sub[j]->wsub);
						menu->sub[j]->wsub = NULL;
					}

					p.x = MwGetInteger(handle, MwNwidth);
					p.y = rc.y - 3;

					menu->sub[i]->wsub = MwCreateWidget(MwSubMenuClass, "submenu", handle, 0, 0, 0, 0);
					MwSubMenuAppear(menu->sub[i]->wsub, menu->sub[i], &p, 0);
					i = -1;
				} else if(menu->sub[i]->wsub != NULL && arrlen(menu->sub[i]->sub) > 0) {
					while(w->parent->widget_class == MwSubMenuClass) w = w->parent;

					MwDestroyWidget(menu->sub[i]->wsub);
					menu->sub[i]->wsub = NULL;

					MwForceRender(w->parent);

					MwForceRender(handle);
				} else if(strcmp(menu->sub[i]->name, "----") != 0 && arrlen(menu->sub[i]->sub) == 0) {
					while(w->parent->widget_class == MwSubMenuClass) w = w->parent;

					MwDestroyWidget(w);
					((MwMenu)w->internal)->wsub = NULL;

					MwForceRender(w->parent);

					MwDispatchUserHandler(w->parent, MwNmenuHandler, menu->sub[i]);
				}
			}

			rc.y += rc.height + 3;
		}
	}
}

static void mwSubMenuAppearImpl(MwWidget handle, MwMenu menu, MwPoint* point, int diff_calc) {
	MwRect	rc, sz;
	MwPoint p = *point;

	MwSubMenuGetSize(handle, menu, &sz);

	MwGetScreenSize(handle, &rc);

	handle->internal = menu;

	if(diff_calc) {
		p.y = p.y - sz.height;
	}

	MwLLMakeToolWindow(handle->lowlevel);
	MwLLDetach(handle->lowlevel, &p);

	if(handle->lowlevel->common.coordinate_type == MwCOORDINATE_GLOBAL) {
		if(MwGetInteger(handle, MwNy) + sz.height > rc.height) {
			MwVaApply(handle,
				  MwNy, rc.height - sz.height,
				  NULL);
		}
	}

	MwLLEndStateChange(handle->lowlevel);

	MwVaApply(handle,
		  MwNwidth, sz.width,
		  MwNheight, sz.height,
		  NULL);
}

static void mwSubMenuGetSizeImpl(MwWidget handle, MwMenu menu, MwRect* rect) {
	int i;

	rect->width  = 0;
	rect->height = 0;

	for(i = 0; i < arrlen(menu->sub); i++) {
		if(strcmp(menu->sub[i]->name, "----") == 0) {
			rect->height += 2 + 2;
		} else {
			int tw = MwTextWidth(handle, NULL, menu->sub[i]->name);
			rect->height += MwTextHeight(handle, NULL, menu->sub[i]->name) + 3;
			if(tw > rect->width) {
				rect->width = tw;
			}
		}
	}

	rect->width += MwGetInteger(handle, MwNleftPadding);

	rect->width += 10 + 15;
	rect->height += 3;

	rect->width += 16;
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	(void)out;

	if(strcmp(name, "mwSubMenuAppear") == 0) {
		MwMenu	 menu	   = va_arg(va, MwMenu);
		MwPoint* point	   = va_arg(va, MwPoint*);
		int	 diff_calc = va_arg(va, int);
		mwSubMenuAppearImpl(handle, menu, point, diff_calc);
	} else if(strcmp(name, "mwSubMenuGetSize") == 0) {
		MwMenu	menu = va_arg(va, MwMenu);
		MwRect* rect = va_arg(va, MwRect*);
		mwSubMenuGetSizeImpl(handle, menu, rect);
	}
}

MwClassRec MwSubMenuClassRec = {
    wcreate,	    /* create */
    destroy,	    /* destroy */
    draw,	    /* draw */
    click,	    /* click */
    NULL,	    /* parent_resize */
    NULL,	    /* prop_change */
    NULL,	    /* mouse_move */
    MwForceRender2, /* mouse_up */
    MwForceRender2, /* mouse_down */
    NULL,	    /* key */
    func_handler,   /* execute */
    NULL,	    /* tick */
    NULL,	    /* resize */
    NULL,	    /* children_update */
    NULL,	    /* children_prop_change */
    NULL,	    /* clipboard */
    NULL,	    /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwSubMenuClass = &MwSubMenuClassRec;
