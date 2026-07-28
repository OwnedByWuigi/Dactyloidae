#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNorientation, MwHORIZONTAL);

	return 0;
}

static void draw(MwWidget handle) {
	MwRect	  r;
	MwLLColor base = MwParseColor(handle, MwGetText(handle, MwNbackground));

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);
	MwDrawRect(handle, &r, base);

	if(MwGetInteger(handle, MwNorientation) == MwVERTICAL) {
		r.x	= (r.width - 2) / 2;
		r.width = 2;
	} else if(MwGetInteger(handle, MwNorientation) == MwHORIZONTAL) {
		r.y	 = (r.height - 2) / 2;
		r.height = 2;
	}
	MwDrawFrameEx(handle, &r, base, 1, 1, 0, 0);

	MwLLFreeColor(base);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNorientation) == 0) MwForceRender(handle);
}

MwClassRec MwSeparatorClassRec = {
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
MwClass MwSeparatorClass = &MwSeparatorClassRec;
