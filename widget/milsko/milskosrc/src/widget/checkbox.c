#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNchecked, 0);

	return 0;
}

static void draw(MwWidget handle) {
	MwRect	   r;
	MwLLColor  base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLPixmap bgpx = MwGetVoid(handle, MwNbackgroundPixmap);

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);
	MwDrawRect(handle, &r, base);

	if(r.width < r.height) {
		r.height = r.width;
	} else {
		r.width = r.height;
	}
	r.x = (MwGetInteger(handle, MwNwidth) - r.width) / 2;
	r.y = (MwGetInteger(handle, MwNheight) - r.height) / 2;

	MwDrawWidgetBack(handle, &r, base, (handle->pressed || MwGetInteger(handle, MwNchecked)) ? 1 : 0, MwDEFAULT);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);
	if(handle->pressed || MwGetInteger(handle, MwNchecked)) {
		/* TODO: write check mark */
	}

	MwLLFreeColor(base);
}

static void click(MwWidget handle) {
	MwSetInteger(handle, MwNchecked, MwGetInteger(handle, MwNchecked) ? 0 : 1);

	MwDispatchUserHandler(handle, MwNchangedHandler, NULL);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNchecked) == 0) MwForceRender(handle);
}

MwClassRec MwCheckBoxClassRec = {
    wcreate,	    /* create */
    NULL,	    /* destroy */
    draw,	    /* draw */
    click,	    /* click */
    NULL,	    /* parent_resize */
    prop_change,    /* prop_change */
    NULL,	    /* mouse_move */
    MwForceRender2, /* mouse_up */
    MwForceRender2, /* mouse_down */
    NULL,	    /* key */
    NULL,	    /* execute */
    NULL,	    /* tick */
    MwForceRender,  /* resize */
    NULL,	    /* children_update */
    NULL,	    /* children_prop_change */
    NULL,	    /* clipboard */
    NULL,	    /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwCheckBoxClass = &MwCheckBoxClassRec;
