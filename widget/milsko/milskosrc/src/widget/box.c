#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

static int wcreate(MwWidget handle) {
	MwBox b = malloc(sizeof(*b));
	memset(b, 0, sizeof(*b));

	handle->internal = b;

	MwSetDefault(handle);

	MwSetInteger(handle, MwNorientation, MwHORIZONTAL);
	MwSetInteger(handle, MwNmargin, 0);
	MwSetInteger(handle, MwNpadding, 0);
	MwSetInteger(handle, MwNhasBorder, 0);
	MwSetInteger(handle, MwNinverted, 1);
	MwSetInteger(handle, MwNwaitLayout, 0);

	b->layout = 0;

	return 0;
}

static void destroy(MwWidget handle) {
	MwBox b = handle->internal;
	free(b);
}

#define Margin ((i != (arrlen(handle->children) - 1)) ? MwGetInteger(handle, MwNmargin) : 0)

static void layout(MwWidget handle) {
	int i;
	int sum	  = 0;
	int horiz = MwGetInteger(handle, MwNorientation) == MwHORIZONTAL ? 1 : 0;
	int sz	  = MwGetInteger(handle, horiz ? MwNwidth : MwNheight) - (MwGetInteger(handle, MwNpadding) + (MwGetInteger(handle, MwNhasBorder) ? MwDefaultBorderWidth(handle) : 0)) * 2;
	int fsz	  = MwGetInteger(handle, horiz ? MwNheight : MwNwidth) - (MwGetInteger(handle, MwNpadding) + (MwGetInteger(handle, MwNhasBorder) ? MwDefaultBorderWidth(handle) : 0)) * 2;
	int sk	  = MwGetInteger(handle, MwNpadding) + (MwGetInteger(handle, MwNhasBorder) ? MwDefaultBorderWidth(handle) : 0);

	for(i = 0; i < arrlen(handle->children); i++) {
		int n = MwGetInteger(handle->children[i], MwNratio);
		int s = MwGetInteger(handle->children[i], MwNfixedSize);
		if(n == MwDEFAULT) n = 1;

		if(handle->children[i]->destroyed) continue;

		if(s != MwDEFAULT) {
			sz -= s + Margin;
		} else {
			sum += n;
			sz -= Margin;
		}
	}

	for(i = 0; i < arrlen(handle->children); i++) {
		int n = MwGetInteger(handle->children[i], MwNratio);
		int s = MwGetInteger(handle->children[i], MwNfixedSize);
		int wsz;
		if(n == MwDEFAULT) n = 1;

		if(handle->children[i]->destroyed) continue;

		if(s != MwDEFAULT) {
			wsz = s;
		} else {
			wsz = sz * n / sum;
		}

		MwVaApply(handle->children[i],
			  horiz ? MwNx : MwNy, sk,													   /* this is what gets changed */
			  horiz ? MwNy : MwNx, MwGetInteger(handle, MwNpadding) + (MwGetInteger(handle, MwNhasBorder) ? MwDefaultBorderWidth(handle) : 0), /* fixed between widgets */
			  horiz ? MwNwidth : MwNheight, wsz,												   /* this is what gets changed */
			  horiz ? MwNheight : MwNwidth, fsz,												   /* fixed between widgets */
			  NULL);
		sk += wsz + Margin;
	}
}

static void draw(MwWidget handle) {
	MwRect	  r;
	MwLLColor base = MwParseColor(handle, MwGetText(handle, MwNbackground));

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	if(MwGetInteger(handle, MwNhasBorder)) {
		MwDrawFrame(handle, &r, base, MwGetInteger(handle, MwNinverted));
	}

	MwDrawRect(handle, &r, base);

	MwLLFreeColor(base);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNorientation) == 0 || strcmp(key, MwNhasBorder) == 0 || strcmp(key, MwNinverted) == 0 || strcmp(key, MwNpadding) == 0 || strcmp(key, MwNmargin) == 0) {
		MwBox b	  = handle->internal;
		b->layout = 1;

		MwForceRender(handle);
	}
}

static void tick(MwWidget handle) {
	MwBox b = handle->internal;

	if(b->layout && !MwGetInteger(handle, MwNwaitLayout)) {
		layout(handle);

		b->layout = 0;
	}
}

static void resize(MwWidget handle) {
	MwBox b	  = handle->internal;
	b->layout = 1;

	MwForceRender(handle);
}

static void children_update(MwWidget handle, MwWidget child, int new_child) {
	MwBox b = handle->internal;

	(void)child;
	(void)new_child;

	b->layout = 1;

	MwForceRender(handle);
}

static void children_prop_change(MwWidget handle, MwWidget child, const char* key) {
	(void)child;

	if(strcmp(key, MwNratio) == 0 || strcmp(key, MwNfixedSize) == 0) {
		MwBox b	  = handle->internal;
		b->layout = 1;

		MwForceRender(handle);
	}
}

MwClassRec MwBoxClassRec = {
    wcreate,		  /* create */
    destroy,		  /* destroy */
    draw,		  /* draw */
    NULL,		  /* click */
    NULL,		  /* parent_resize */
    prop_change,	  /* prop_change */
    NULL,		  /* mouse_move */
    NULL,		  /* mouse_up */
    NULL,		  /* mouse_down */
    NULL,		  /* key */
    NULL,		  /* execute */
    tick,		  /* tick */
    resize,		  /* resize */
    children_update,	  /* children_update */
    children_prop_change, /* children_prop_change */
    NULL,		  /* clipboard */
    NULL,		  /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwBoxClass = &MwBoxClassRec;
