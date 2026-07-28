#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNhasBorder, 0);
	MwSetInteger(handle, MwNinverted, 1);
	MwSetInteger(handle, MwNfillArea, 1);

	return 0;
}

static void draw(MwWidget handle) {
	MwRect	   r;
	MwLLPixmap px	= MwGetVoid(handle, MwNpixmap);
	MwLLColor  base = MwParseColor(handle, MwGetText(handle, MwNbackground));

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	if(MwGetInteger(handle, MwNhasBorder)) MwDrawFrame(handle, &r, base, MwGetInteger(handle, MwNinverted));

	MwDrawRect(handle, &r, base);
	if(px != NULL) {
		if(!MwGetInteger(handle, MwNfillArea)) {
			r.x	 = (r.width - px->common.width) / 2;
			r.y	 = (r.height - px->common.height) / 2;
			r.width	 = px->common.width;
			r.height = px->common.height;
		}

		MwLLDrawPixmap(handle->lowlevel, &r, px);
	}

	MwLLFreeColor(base);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNpixmap) == 0 || strcmp(key, MwNhasBorder) == 0 || strcmp(key, MwNinverted) == 0) MwForceRender(handle);
}

MwClassRec MwImageClassRec = {
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
MwClass MwImageClass = &MwImageClassRec;
