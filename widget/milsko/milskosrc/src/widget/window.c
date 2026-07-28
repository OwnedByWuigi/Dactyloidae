#include <Mw/Milsko.h>

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNhasBorder, 0);
	MwSetInteger(handle, MwNinverted, 1);

	return 0;
}

static void draw(MwWidget handle) {
	MwLLColor c = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwRect	  r;

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	if(MwGetInteger(handle, MwNhasBorder)) MwDrawFrame(handle, &r, c, MwGetInteger(handle, MwNinverted));

	MwDrawRect(handle, &r, c);

	MwLLFreeColor(c);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNhasBorder) == 0 || strcmp(key, MwNinverted) == 0) MwForceRender(handle);
}

static void mwWindowMakeBorderlessImpl(MwWidget handle, int toggle) {
	MwLLBeginStateChange(handle->lowlevel);
	MwLLMakeBorderless(handle->lowlevel, toggle);
	MwLLEndStateChange(handle->lowlevel);
}

static void mwWindowShouldCloseImpl(MwWidget handle, MwBool* out) {
	*out = handle->close;
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	(void)out;

	if(strcmp(name, "mwWindowMakeBorderless") == 0) {
		int toggle = va_arg(va, int);
		mwWindowMakeBorderlessImpl(handle, toggle);
	}
	if(strcmp(name, "mwWindowShouldClose") == 0) {
		MwBool* out = va_arg(va, MwBool*);
		mwWindowShouldCloseImpl(handle, out);
	}
}
MwClassRec MwWindowClassRec = {
    wcreate,	  /* create */
    NULL,	  /* destroy */
    draw,	  /* draw */
    NULL,	  /* click */
    NULL,	  /* parent_resize */
    prop_change,  /* prop_change */
    NULL,	  /* mouse_move */
    NULL,	  /* mouse_up */
    NULL,	  /* mouse_down */
    NULL,	  /* key */
    func_handler, /* execute */
    NULL,	  /* tick */
    NULL,	  /* resize */
    NULL,	  /* children_update */
    NULL,	  /* children_prop_change */
    NULL,	  /* clipboard */
    NULL,	  /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwWindowClass = &MwWindowClassRec;
