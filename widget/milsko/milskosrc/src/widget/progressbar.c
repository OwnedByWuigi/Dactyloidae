#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);
	MwVaApply(handle,
		  MwNminValue, 0,
		  MwNmaxValue, 100,
		  MwNvalue, 0,
		  NULL);

	return 0;
}

static void draw(MwWidget handle) {
	MwRect	   r;
	MwLLColor  base = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor  fill = MwParseColor(handle, MwGetText(handle, MwNforeground));
	double	   w;
	MwLLPixmap bgpx = MwGetVoid(handle, MwNbackgroundPixmap);

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	w = MwGetInteger(handle, MwNvalue) - MwGetInteger(handle, MwNminValue);
	w = w / (MwGetInteger(handle, MwNmaxValue) - MwGetInteger(handle, MwNminValue));

	MwDrawWidgetBack(handle, &r, base, 1, MwDEFAULT);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);

	r.x += MwDefaultBorderWidth(handle);
	r.y += MwDefaultBorderWidth(handle);
	r.width -= MwDefaultBorderWidth(handle) * 2;
	r.height -= MwDefaultBorderWidth(handle) * 2;

	r.width = r.width * w;
	MwDrawRect(handle, &r, fill);

	MwLLFreeColor(fill);
	MwLLFreeColor(base);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNminValue) == 0 || strcmp(key, MwNmaxValue) == 0 || strcmp(key, MwNvalue) == 0) MwForceRender(handle);
}

MwClassRec MwProgressBarClassRec = {
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
MwClass MwProgressBarClass = &MwProgressBarClassRec;
