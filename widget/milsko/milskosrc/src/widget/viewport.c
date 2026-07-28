#include <Mw/Milsko.h>

static void MWAPI vscroll_changed(MwWidget handle, void* user, void* call) {
	MwViewport vp = user;

	(void)handle;
	(void)call;

	vp->vchanged = 1;
}

static void MWAPI hscroll_changed(MwWidget handle, void* user, void* call) {
	MwViewport vp = user;

	(void)handle;
	(void)call;

	vp->hchanged = 1;
}

static void resize(MwWidget handle) {
	MwViewport vp = handle->internal;
	int	   w  = MwGetInteger(handle, MwNwidth);
	int	   h  = MwGetInteger(handle, MwNheight);
	int	   iw;
	int	   ih;
	if(vp->vscroll == NULL) {
		vp->vscroll = MwVaCreateWidget(MwScrollBarClass, "vscroll", handle, w - 16, 0, 16, h - 16, NULL);
		MwAddUserHandler(vp->vscroll, MwNchangedHandler, vscroll_changed, vp);
	} else {
		MwVaApply(vp->vscroll,
			  MwNx, w - 16,
			  MwNy, 0,
			  MwNwidth, 16,
			  MwNheight, h - 16,
			  NULL);
	}
	if(vp->hscroll == NULL) {
		vp->hscroll = MwVaCreateWidget(MwScrollBarClass, "hscroll", handle, 0, h - 16, w - 16, 16, MwNorientation, MwHORIZONTAL, NULL);
		MwAddUserHandler(vp->hscroll, MwNchangedHandler, hscroll_changed, vp);
	} else {
		MwVaApply(vp->hscroll,
			  MwNx, 0,
			  MwNy, h - 16,
			  MwNwidth, w - 16,
			  MwNheight, 16,
			  NULL);
	}
	if(vp->frame == NULL) {
		vp->frame = MwCreateWidget(MwFrameClass, "frame", handle, 0, 0, w - 16, h - 16);
	} else {
		MwVaApply(vp->frame,
			  MwNx, 0,
			  MwNy, 0,
			  MwNwidth, w - 16,
			  MwNheight, h - 16,
			  NULL);
	}
	if(vp->inframe == NULL) {
		vp->inframe = MwCreateWidget(MwFrameClass, "inframe", vp->frame, 0, 0, w - 16, h - 16);
	}

	iw = MwGetInteger(vp->inframe, MwNwidth);
	ih = MwGetInteger(vp->inframe, MwNheight);

	MwVaApply(vp->vscroll,
		  MwNareaShown, h - 16,
		  MwNmaxValue, ih,
		  NULL);

	MwVaApply(vp->hscroll,
		  MwNareaShown, w - 16,
		  MwNmaxValue, iw,
		  NULL);
}

static int wcreate(MwWidget handle) {
	MwViewport vp = malloc(sizeof(*vp));
	memset(vp, 0, sizeof(*vp));

	handle->internal = vp;

	MwSetDefault(handle);

	vp->vchanged = 0;
	vp->hchanged = 0;

	resize(handle);

	return 0;
}

static void destroy(MwWidget handle) {
	free(handle->internal);
}

static void draw(MwWidget handle) {
	MwRect	  r;
	MwLLColor base = MwParseColor(handle, MwGetText(handle, MwNbackground));

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawRect(handle, &r, base);

	MwLLFreeColor(base);
}

static void prop_change(MwWidget handle, const char* prop) {
	if(strcmp(prop, MwNwidth) == 0 || strcmp(prop, MwNheight) == 0) resize(handle);
}

static MwWidget mwViewportGetViewportImpl(MwWidget handle) {
	MwViewport vp = handle->internal;

	return vp->inframe;
}

static void mwViewportSetSizeImpl(MwWidget handle, int w, int h) {
	MwViewport vp = handle->internal;

	MwVaApply(vp->inframe,
		  MwNwidth, w,
		  MwNheight, h,
		  NULL);

	resize(handle);
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	if(strcmp(name, "mwViewportGetViewport") == 0) {
		*(MwWidget*)out = mwViewportGetViewportImpl(handle);
	}
	if(strcmp(name, "mwViewportSetSize") == 0) {
		int w = va_arg(va, int);
		int h = va_arg(va, int);
		mwViewportSetSizeImpl(handle, w, h);
	}
}

static void tick(MwWidget handle) {
	MwViewport vp = handle->internal;

	if(vp->vchanged) {
		int v, mv, l;

		vp->vchanged = 0;

		v  = MwGetInteger(vp->vscroll, MwNvalue);
		mv = MwGetInteger(vp->vscroll, MwNmaxValue);
		l  = MwGetInteger(vp->frame, MwNheight);
		v  = (mv - l) * (double)v / mv;

		if(v < 0) v = 0;
		MwVaApply(vp->inframe,
			  MwNy, -v,
			  NULL);
	}
	if(vp->hchanged) {
		int v, mv, l;

		vp->vchanged = 0;

		v  = MwGetInteger(vp->hscroll, MwNvalue);
		mv = MwGetInteger(vp->hscroll, MwNmaxValue);
		l  = MwGetInteger(vp->frame, MwNwidth);
		v  = (mv - l) * (double)v / mv;

		if(v < 0) v = 0;
		MwVaApply(vp->inframe,
			  MwNx, -v,
			  NULL);
	}
}

MwClassRec MwViewportClassRec = {
    wcreate,	  /* create */
    destroy,	  /* destroy */
    draw,	  /* draw */
    NULL,	  /* click */
    NULL,	  /* parent_resize */
    prop_change,  /* prop_change */
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
    NULL,	  /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwViewportClass = &MwViewportClassRec;
