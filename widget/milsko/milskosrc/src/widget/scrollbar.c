#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwScrollBar scr = malloc(sizeof(*scr));

	handle->internal = scr;

	MwSetDefault(handle);
	MwVaApply(handle,
		  MwNminValue, 0,
		  MwNmaxValue, 100,
		  MwNvalue, 0,
		  MwNareaShown, 50,
		  MwNorientation, MwVERTICAL,
		  MwNshowArrows, 1,
		  NULL);

	return 0;
}

static void destroy(MwWidget handle) {
	free(handle->internal);
}

static int calc_length(MwWidget handle) {
	int max	 = MwScrollBarGetVisibleLength(handle);
	int len	 = MwGetInteger(handle, MwNmaxValue) - MwGetInteger(handle, MwNminValue);
	int area = MwGetInteger(handle, MwNareaShown);
	if(area > len) area = len;

	return max * (double)area / len;
}

static int calc_position(MwWidget handle) {
	int max = MwScrollBarGetVisibleLength(handle);
	int len = MwGetInteger(handle, MwNmaxValue) - MwGetInteger(handle, MwNminValue);
	int val = MwGetInteger(handle, MwNvalue);

	return (max - calc_length(handle)) * (double)val / len;
}

static void add_value(MwWidget handle, int mul) {
	int val = MwGetInteger(handle, MwNvalue);
	int min = MwGetInteger(handle, MwNminValue);
	int max = MwGetInteger(handle, MwNmaxValue);

	val += mul * MwGetInteger(handle, MwNareaShown);

	if(val < min) val = min;
	if(val > max) val = max;

	MwSetInteger(handle, MwNvalue, val);
	MwDispatchUserHandler(handle, MwNchangedHandler, NULL);
}

static void draw(MwWidget handle) {
	MwRect	    r, rt, rbar;
	MwLLColor   base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor   dark = MwLightenColor(handle, base, -64, -64, -64);
	MwScrollBar scr	 = handle->internal;
	int or ;
	int	   uy, dy, ux, dx;
	MwLLPixmap bgpx = MwGetVoid(handle, MwNbackgroundPixmap);

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	uy = r.width;
	dy = r.height - r.width;

	ux = r.height;
	dx = r.width - r.height;

	MwDrawWidgetBack(handle, &r, dark, 1, MwDEFAULT);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);

	rt = r;

	or = MwGetInteger(handle, MwNorientation);
	if(or == MwVERTICAL) {
		rt.height = rt.width;

		rt.y = r.y;
		if(MwGetInteger(handle, MwNshowArrows)) {
			MwDrawTriangle(handle, &rt, base, (handle->pressed && scr->point.y <= uy) ? 1 : 0, MwNORTH);
			if(handle->pressed && scr->point.y <= uy) {
				add_value(handle, -1);
			}
		} else {
			rt.height = 0;
		}

		rbar.width  = r.width;
		rbar.height = calc_length(handle);
		rbar.x	    = r.x;
		rbar.y	    = r.y + rt.height + calc_position(handle);

		rt.y = r.y + r.height - rt.height;
		if(MwGetInteger(handle, MwNshowArrows)) {
			MwDrawTriangle(handle, &rt, base, (handle->pressed && scr->point.y >= dy) ? 1 : 0, MwSOUTH);
			if(handle->pressed && scr->point.y >= dy) {
				add_value(handle, 1);
			}
		}
	} else if(or == MwHORIZONTAL) {
		rt.width = rt.height;

		rt.x = r.x;
		if(MwGetInteger(handle, MwNshowArrows)) {
			MwDrawTriangle(handle, &rt, base, (handle->pressed && scr->point.x <= ux) ? 1 : 0, MwWEST);
			if(handle->pressed && scr->point.x <= ux) {
				add_value(handle, -1);
			}
		} else {
			rt.width = 0;
		}

		rbar.width  = calc_length(handle);
		rbar.height = r.height;
		rbar.x	    = r.x + rt.width + calc_position(handle);
		rbar.y	    = r.y;

		rt.x = r.x + r.width - rt.width;
		if(MwGetInteger(handle, MwNshowArrows)) {
			MwDrawTriangle(handle, &rt, base, (handle->pressed && scr->point.x >= dx) ? 1 : 0, MwEAST);
			if(handle->pressed && scr->point.x >= dx) {
				add_value(handle, 1);
			}
		}
	}

	if(rbar.height >= 0) MwDrawWidgetBack(handle, &rbar, base, 0, MwDEFAULT);

	MwLLFreeColor(dark);
	MwLLFreeColor(base);
}

static void mouse_move(MwWidget handle) {
	int	    or	= MwGetInteger(handle, MwNorientation);
	MwScrollBar scr = handle->internal;

	if(!handle->pressed) return;

	if(scr->drag) {
		int    l   = 0;
		double len = MwScrollBarGetVisibleLength(handle) - calc_length(handle);
		int    min = MwGetInteger(handle, MwNminValue);
		int    max = MwGetInteger(handle, MwNmaxValue);

		if(or == MwVERTICAL) {
			l = handle->mouse_point.y + scr->pos;
		} else if(or == MwHORIZONTAL) {
			l = handle->mouse_point.x + scr->pos;
		}

		len = l / len;
		if(len < 0) len = 0;
		if(len > 1) len = 1;
		MwSetInteger(handle, MwNvalue, (int)((max - min) * len - min));
		MwDispatchUserHandler(handle, MwNchangedHandler, NULL);

		MwForceRender(handle);
	}
}

static void mouse_down(MwWidget handle, void* ptr) {
	int	    ww	= MwGetInteger(handle, MwNwidth);
	int	    wh	= MwGetInteger(handle, MwNheight);
	int	    or	= MwGetInteger(handle, MwNorientation);
	MwScrollBar scr = handle->internal;
	MwMouse*    m	= ptr;

	if(m->button == MwMOUSE_WHEELUP) {
		int min	 = MwGetInteger(handle, MwNminValue);
		int val	 = MwGetInteger(handle, MwNvalue);
		int diff = MwGetInteger(handle, MwNareaShown);

		val -= diff;

		if(val < min) val = min;

		MwSetInteger(handle, MwNvalue, val);
		MwSetInteger(handle, MwNchangedBy, -diff);
		MwDispatchUserHandler(handle, MwNchangedHandler, NULL);
	} else if(m->button == MwMOUSE_WHEELDOWN) {
		int max	 = MwGetInteger(handle, MwNmaxValue);
		int val	 = MwGetInteger(handle, MwNvalue);
		int diff = MwGetInteger(handle, MwNareaShown);

		val += diff;

		if(val > max) val = max;

		MwSetInteger(handle, MwNvalue, val);
		MwSetInteger(handle, MwNchangedBy, diff);
		MwDispatchUserHandler(handle, MwNchangedHandler, NULL);
	}
	if(m->button != MwMOUSE_LEFT) return;

	scr->point = handle->mouse_point;
	scr->drag  = 0;
	if(or == MwVERTICAL) {
		int tri = (ww - MwDefaultBorderWidth(handle) * 2) + MwDefaultBorderWidth(handle);
		if(!MwGetInteger(handle, MwNshowArrows)) tri = 0;
		if(tri <= scr->point.y && scr->point.y <= (wh - tri)) {
			scr->drag = 1;
			scr->pos  = calc_position(handle) - scr->point.y;
		}
	} else if(or == MwHORIZONTAL) {
		int tri = (wh - MwDefaultBorderWidth(handle) * 2) + MwDefaultBorderWidth(handle);
		if(!MwGetInteger(handle, MwNshowArrows)) tri = 0;
		if(tri <= scr->point.x && scr->point.x <= (ww - tri)) {
			scr->drag = 1;
			scr->pos  = calc_position(handle) - scr->point.x;
		}
	}

	MwForceRender(handle);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNminValue) == 0 || strcmp(key, MwNvalue) == 0 || strcmp(key, MwNmaxValue) == 0) {
		if(MwGetInteger(handle, MwNvalue) > MwGetInteger(handle, MwNmaxValue)) MwSetInteger(handle, MwNvalue, MwGetInteger(handle, MwNmaxValue));
		MwForceRender(handle);
	}
	if(strcmp(key, MwNshowArrows) == 0 || strcmp(key, MwNareaShown) == 0) {
		MwForceRender(handle);
	}
}

static int mwScrollBarGetVisibleLengthImpl(MwWidget handle) {
	int ww	= MwGetInteger(handle, MwNwidth);
	int wh	= MwGetInteger(handle, MwNheight);
	int or	= MwGetInteger(handle, MwNorientation);
	int tri = 0;
	int s	= 0;

	if(or == MwVERTICAL) {
		tri = (ww - MwDefaultBorderWidth(handle) * 2) * 2;
		s   = wh;
	} else if(or == MwHORIZONTAL) {
		tri = (wh - MwDefaultBorderWidth(handle) * 2) * 2;
		s   = ww;
	}
	if(!MwGetInteger(handle, MwNshowArrows)) tri = 0;
	return s - tri - MwDefaultBorderWidth(handle) * 2;
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	(void)va;

	if(strcmp(name, "mwScrollBarGetVisibleLength") == 0) {
		*(int*)out = mwScrollBarGetVisibleLengthImpl(handle);
	}
}

MwClassRec MwScrollBarClassRec = {
    wcreate,	    /* create */
    destroy,	    /* destroy */
    draw,	    /* draw */
    NULL,	    /* click */
    NULL,	    /* parent_resize */
    prop_change,    /* prop_change */
    mouse_move,	    /* mouse_move */
    MwForceRender2, /* mouse_up */
    mouse_down,	    /* mouse_down */
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
MwClass MwScrollBarClass = &MwScrollBarClassRec;
