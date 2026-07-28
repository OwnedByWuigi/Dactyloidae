#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

static int wcreate(MwWidget handle) {
	MwTable t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));

	handle->internal = t;

	MwSetDefault(handle);

	MwSetInteger(handle, MwNcolumns, 1);
	MwSetInteger(handle, MwNmargin, 0);
	MwSetInteger(handle, MwNpadding, 0);
	MwSetInteger(handle, MwNhasBorder, 0);
	MwSetInteger(handle, MwNinverted, 1);
	MwSetInteger(handle, MwNwaitLayout, 0);

	t->layout = 0;

	return 0;
}

static void destroy(MwWidget handle) {
	MwTable t = handle->internal;

	arrfree(t->widgets);

	free(t);
}

typedef struct intkv {
	int key;
	int value;
} intkv_t;

static void put_intkv(intkv_t** kv, int x, int y, int c, int r) {
	int k = (x << 16) | y;
	int v = (c << 16) | r;

	hmput(*kv, k, v);
}

static int get_intkv(intkv_t* kv, int x, int y) {
	int k = (x << 16) | y;

	return hmget(kv, k);
}

#define LAYOUT_BEGIN \
	hmfree(kv); \
	hmdefault(kv, 0); \
	n = 0; \
	x = bw; \
	y = bw; \
	for(i = 0; i < arrlen(handle->children); i++) { \
		MwRect	 rc; \
		MwWidget hc = handle->children[i]; \
		int	 cs = MwGetInteger(hc, MwNcolumnSpan); \
		int	 rs = MwGetInteger(hc, MwNrowSpan); \
		int	 w, h; \
		int	 cy; \
		int	 s = MwGetInteger(handle, MwNmargin); \
		int	 v = n; \
\
		if(cs == MwDEFAULT) cs = 1; \
		if(rs == MwDEFAULT) rs = 1; \
\
		w = cs * cw + (cs - 1) * s; \
		h = rs * ch + (rs - 1) * s; \
\
		MwGetFrame(hc, &rc); \
\
		if(hc == child || rc.x != x || rc.y != y || rc.width != w || rc.height != h) {

#define LAYOUT_END(label) \
	} \
\
	put_intkv(&kv, n % c, n / c, cs, rs); \
\
	n += cs; \
	repeat_##label :; \
	if((n % c) == 0) { \
		x = bw; \
		y += ch + s; \
	} else { \
		for(; v < n; v++) { \
			x += cw + s; \
		} \
	} \
\
	for(cy = (n / c); cy >= 0; cy--) { \
		int v = get_intkv(kv, n % c, cy) & 0xffff; \
\
		if(cy <= (n / c) && (n / c) <= (cy + v - 1)) { \
			v = n; \
			n += get_intkv(kv, n % c, cy) >> 16; \
			goto repeat_##label; \
		} \
	} \
	}

static void layout_widget(MwWidget handle, MwWidget child) {
	int	 i;
	int	 c  = MwGetInteger(handle, MwNcolumns);
	int	 bw = (MwGetInteger(handle, MwNhasBorder) ? MwDefaultBorderWidth(handle) : 0) + MwGetInteger(handle, MwNpadding);
	int	 m  = MwGetInteger(handle, MwNmargin);
	int	 n, x, y, cw = 0, ch = 0;
	int	 gcs;
	intkv_t* kv = NULL;

	LAYOUT_BEGIN;
	LAYOUT_END(1);

	gcs = (n + c - 1) / c;
	cw  = (MwGetInteger(handle, MwNwidth) - bw * 2 - (m * (c - 1))) / c;
	ch  = (MwGetInteger(handle, MwNheight) - m * (gcs - 1) - bw * 2) / gcs;

	LAYOUT_BEGIN;
	MwVaApply(hc,
		  MwNx, x,
		  MwNy, y,
		  MwNwidth, w,
		  MwNheight, h,
		  NULL);
	LAYOUT_END(2);

	hmfree(kv);
}

static void layout(MwWidget handle) {
	MwTable t = handle->internal;
	int	i;

	if(arrlen(t->widgets) > 0) {
		for(i = 0; i < arrlen(t->widgets); i++) layout_widget(handle, t->widgets[i]);
	} else {
		for(i = 0; i < arrlen(handle->children[i]); i++) layout_widget(handle, handle->children[i]);
	}
}

static void draw(MwWidget handle) {
	MwBoxClass->draw(handle);
}

static void tick(MwWidget handle) {
	MwTable t = handle->internal;

	if(t->layout && !MwGetInteger(handle, MwNwaitLayout)) {
		layout(handle);

		t->layout = 0;
	}
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNcolumns) == 0 || strcmp(key, MwNhasBorder) == 0 || strcmp(key, MwNinverted) == 0 || strcmp(key, MwNpadding) == 0 || strcmp(key, MwNmargin) == 0) {
		MwTable t = handle->internal;
		t->layout = 1;

		MwForceRender(handle);
	}
}

static void resize(MwWidget handle) {
	MwTable t = handle->internal;
	t->layout = 1;

	MwForceRender(handle);
}

static void children_update(MwWidget handle, MwWidget child, int new_child) {
	MwTable t = handle->internal;
	t->layout = 1;

	if(new_child) {
		arrput(t->widgets, child);
	} else {
		int i;

		for(i = 0; i < arrlen(t->widgets); i++) {
			if(t->widgets[i] == child) {
				arrdel(t->widgets, i);
				i = -1;
			}
		}
	}

	MwForceRender(handle);
}

static void children_prop_change(MwWidget handle, MwWidget child, const char* key) {
	(void)child;

	if(strcmp(key, MwNcolumnSpan) == 0 || strcmp(key, MwNrowSpan) == 0) {
		MwTable t = handle->internal;
		t->layout = 1;

		arrput(t->widgets, child);

		MwForceRender(handle);
	}
}

MwClassRec MwTableClassRec = {
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
MwClass MwTableClass = &MwTableClassRec;
