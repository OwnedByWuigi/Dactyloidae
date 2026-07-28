#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNflat, 0);
	MwSetInteger(handle, MwNpadding, 0);
	MwSetInteger(handle, MwNfillArea, 1);

	return 0;
}

static void draw(MwWidget handle) {
	MwRect	    r;
	MwPoint	    point;
	MwLLColor   base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor   text = MwParseColor(handle, MwGetText(handle, MwNforeground));
	const char* str	 = MwGetText(handle, MwNtext);
	MwLLPixmap  px	 = MwGetVoid(handle, MwNpixmap);
	MwLLPixmap  bgpx = MwGetVoid(handle, MwNbackgroundPixmap);
	int	    inv;

	if(str == NULL) str = "";

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	if(MwGetInteger(handle, MwNflat)) {
		if(handle->pressed || ((inv = MwGetInteger(handle, MwNforceInverted)) != MwDEFAULT && inv)) {
			MwDrawWidgetBack(handle, &r, base, handle->pressed, MwDEFAULT);
		} else {
			MwDrawRect(handle, &r, base);
		}
	} else {
		MwDrawWidgetBack(handle, &r, base, handle->pressed, MwDEFAULT);
	}
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);
	if(MwGetInteger(handle, MwNflat) && !(handle->pressed || ((inv = MwGetInteger(handle, MwNforceInverted)) != MwDEFAULT && inv))) {
		r.x += MwDefaultBorderWidth(handle);
		r.y += MwDefaultBorderWidth(handle);
		r.width -= MwDefaultBorderWidth(handle) * 2;
		r.height -= MwDefaultBorderWidth(handle) * 2;
	}

	if(px != NULL) {
		int ow = r.width;
		int oh = r.height;

		if(MwGetInteger(handle, MwNfillArea)) {
			double sw = (double)ow / px->common.width;
			double sh = (double)oh / px->common.height;

			if(sw < sh) {
				r.width	 = px->common.width * sw;
				r.height = px->common.height * sw;
			} else {
				r.width	 = px->common.width * sh;
				r.height = px->common.height * sh;
			}
			r.width -= MwGetInteger(handle, MwNpadding) * 2;
			r.height -= MwGetInteger(handle, MwNpadding) * 2;
		} else {
			r.width	 = px->common.width;
			r.height = px->common.height;
		}

		r.x += (double)(ow - r.width) / 2;
		r.y += (double)(oh - r.height) / 2;

		MwLLDrawPixmap(handle->lowlevel, &r, px);
	} else {
		point.x = r.x + r.width / 2;
		point.y = r.x + r.height / 2;

		MwDrawText(handle, NULL, &point, str, MwALIGNMENT_CENTER, text);
	}

	MwLLFreeColor(text);
	MwLLFreeColor(base);
}

static void click(MwWidget handle) {
	MwDispatchUserHandler(handle, MwNactivateHandler, NULL);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNtext) == 0 || strcmp(key, MwNpixmap) == 0 || strcmp(key, MwNflat) == 0 || strcmp(key, MwNpadding) == 0 || strcmp(key, MwNfillArea) == 0) MwForceRender(handle);
}

MwClassRec MwButtonClassRec = {
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
    NULL,	    /* resize */
    NULL,	    /* children_update */
    NULL,	    /* children_prop_change */
    NULL,	    /* clipboard */
    NULL,	    /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwButtonClass = &MwButtonClassRec;
