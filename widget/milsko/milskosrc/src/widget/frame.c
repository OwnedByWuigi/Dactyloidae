#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNhasBorder, 0);
	MwSetInteger(handle, MwNinverted, 1);

	return 0;
}

static void draw(MwWidget handle) {
	MwRect	   fr;
	MwRect	   rr;
	MwLLColor  base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	int	   inverted;
	MwLLPixmap bgpx = MwGetVoid(handle, MwNbackgroundPixmap);

	if(MwGetInteger(handle, MwNhasBorder)) {
		inverted  = MwGetInteger(handle, MwNinverted);
		fr.x	  = 0;
		fr.y	  = 0;
		fr.width  = MwGetInteger(handle, MwNwidth);
		fr.height = MwGetInteger(handle, MwNheight);
		MwDrawFrame(handle, &fr, base, inverted);

		rr.x	  = MwDefaultBorderWidth(handle);
		rr.y	  = MwDefaultBorderWidth(handle);
		rr.width  = MwGetInteger(handle, MwNwidth) - (MwDefaultBorderWidth(handle) * 2);
		rr.height = MwGetInteger(handle, MwNheight) - (MwDefaultBorderWidth(handle) * 2);
	} else {
		rr.x	  = 0;
		rr.y	  = 0;
		rr.width  = MwGetInteger(handle, MwNwidth);
		rr.height = MwGetInteger(handle, MwNheight);
	}

	MwDrawRect(handle, &rr, base);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &rr, bgpx);

	MwLLFreeColor(base);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNhasBorder) == 0 || strcmp(key, MwNinverted) == 0) MwForceRender(handle);
}

MwClassRec MwFrameClassRec = {
    wcreate,	 /* create */
    NULL,	 /* destroy */
    draw,	 /* draw */
    NULL,	 /* click */
    NULL,	 /* parent_resize */
    prop_change, /* prop_change */
    NULL,	 /* mouse_move */
    NULL,	 /* mouse_up */
    NULL,	 /* mouse_down */
    NULL,	 /* key */
    NULL,	 /* execute */
    NULL,	 /* tick */
    NULL,	 /* resize */
    NULL,	 /* children_update */
    NULL,	 /* children_prop_change */
    NULL,	 /* clipboard */
    NULL,	 /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwFrameClass = &MwFrameClassRec;
